//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/mac/SlMode2Selector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace simu5g {

int SlMode2Selector::tProc0Slots(int numerology)
{
    // TS 38.214 Table 8.1.4-1
    static const int table[4] = { 1, 1, 2, 4 };
    return table[std::min(std::max(numerology, 0), 3)];
}

int SlMode2Selector::tProc1Slots(int numerology)
{
    // TS 38.214 Table 8.1.4-2
    static const int table[4] = { 3, 5, 9, 17 };
    return table[std::min(std::max(numerology, 0), 3)];
}

std::vector<char> SlMode2Selector::excludeUnmonitored(SlotIndex windowStart, SlotIndex windowEnd, int lSubch,
        const SlSensingDatabase& db) const
{
    const int windowSlots = (int)(windowEnd - windowStart) + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    std::vector<char> excluded(windowSlots * positions, 0);

    // TS 38.214 §8.1.4 step 5: for a slot the UE did not monitor, assume an SCI
    // was received there reserving ALL subchannels of the pool, at every period
    // the pool allows -- so every candidate in a projected slot is excluded.
    for (SlotIndex m : db.getUnmonitoredSlots()) {
        for (int periodSlots : pool_.allowedPeriodsSlots) {
            if (periodSlots <= 0)
                continue;
            for (SlotIndex s = m + periodSlots; s <= windowEnd; s += periodSlots) {
                if (s < windowStart)
                    continue;
                char *row = &excluded[(s - windowStart) * positions];
                std::fill(row, row + positions, 1);
            }
        }
    }
    return excluded;
}

std::vector<char> SlMode2Selector::excludeSensed(SlotIndex now, SlotIndex windowStart, SlotIndex windowEnd, int lSubch,
        const SlSensingDatabase& db, double thresholdDbm, int txPeriodSlots, int cResel) const
{
    const int windowSlots = (int)(windowEnd - windowStart) + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    std::vector<char> excluded(windowSlots * positions, 0);

    // T_scal, the selection window length, bounds how far a sensed reservation
    // is projected: Q = ceil(T_scal / P_rsvp_RX) occasions (TS 38.214 §8.1.4 6c).
    const int tScalSlots = windowSlots;

    for (const auto& e : db.getEntries()) {
        if (e.rsrpDbm < thresholdDbm || e.reservationPeriodSlots <= 0)
            continue;

        const int q = std::max(1, (e.reservationPeriodSlots < tScalSlots)
                                  ? (int)std::ceil((double)tScalSlots / e.reservationPeriodSlots) : 1);

        for (int i = 1; i <= q; i++) {
            const SlotIndex reserved = e.slot + (SlotIndex)i * e.reservationPeriodSlots;
            if (reserved < windowStart - (SlotIndex)cResel * txPeriodSlots || reserved > windowEnd)
                continue;

            // The candidate itself and each of its cResel-1 repetitions at the
            // transmitter's own reservation period must avoid this reservation:
            // a candidate at slot y collides if y + j*P'_rsvp_TX == reserved for
            // some j in [0, cResel-1], i.e. y == reserved - j*P'_rsvp_TX.
            for (int j = 0; j < std::max(cResel, 1); j++) {
                const SlotIndex candidateSlot = reserved - (SlotIndex)j * txPeriodSlots;
                if (candidateSlot < windowStart || candidateSlot > windowEnd)
                    continue;
                // exclude candidates [first, first+lSubch-1] overlapping the
                // reserved subchannels [firstSubchannel, +numSubchannels-1]
                const int lo = std::max(0, e.firstSubchannel - lSubch + 1);
                const int hi = std::min(positions - 1, e.firstSubchannel + e.numSubchannels - 1);
                char *row = &excluded[(candidateSlot - windowStart) * positions];
                for (int p = lo; p <= hi; p++)
                    row[p] = 1;
                if (txPeriodSlots <= 0)
                    break;   // no repetition train to walk
            }
        }
    }
    (void)now;
    return excluded;
}

SlMode2Selector::Selection SlMode2Selector::select(SlotIndex now, int lSubch, int periodSlots, int periodMs, const SlSensingDatabase& db)
{
    if (lSubch < 1 || lSubch > pool_.numSubchannels)
        throw std::invalid_argument("SlMode2Selector: invalid L_subCH");
    if (pool_.t2 <= pool_.t1)
        throw std::invalid_argument("SlMode2Selector: invalid selection window");

    // Step 1: T1 is up to the UE, but must not be earlier than the physical
    // layer can act (TS 38.214 Table 8.1.4-2).
    const int t1 = std::max(pool_.t1, tProc1Slots(pool_.numerology));
    if (pool_.t2 <= t1)
        throw std::invalid_argument("SlMode2Selector: selection window shorter than T_proc,1");

    const SlotIndex windowStart = now + t1;
    const SlotIndex windowEnd = now + pool_.t2;
    const int windowSlots = (int)(windowEnd - windowStart) + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;

    Selection result;
    result.numCandidates = windowSlots * positions;

    // The counter is drawn before the exclusion because step 6 has to know how
    // many repetitions of the selected resource the transmitter will make.
    result.reselectionCounter = drawReselectionCounter(periodMs);

    const int minSurvivors = (int)std::ceil(pool_.txPercentage * result.numCandidates);

    // Step 5, run once: it does not depend on the RSRP threshold.
    const std::vector<char> unmonitored = excludeUnmonitored(windowStart, windowEnd, lSubch, db);
    int numUnmonitored = 0;
    for (char e : unmonitored)
        numUnmonitored += e;

    // Step 5a: if the unmonitored-slot exclusion alone leaves too little, drop it.
    const bool keepStep5 = (result.numCandidates - numUnmonitored) >= minSurvivors;
    result.step5aApplied = !keepStep5;

    double threshold = pool_.rsrpThresholdDbm;
    std::vector<char> excluded;
    int numExcluded = 0;
    while (true) {
        // Step 4: S_A starts as every candidate; steps 5 and 6 carve it down.
        excluded = keepStep5 ? unmonitored : std::vector<char>(result.numCandidates, 0);
        const std::vector<char> sensed = excludeSensed(now, windowStart, windowEnd, lSubch, db,
                threshold, periodSlots, result.reselectionCounter);
        numExcluded = 0;
        for (size_t i = 0; i < excluded.size(); i++) {
            excluded[i] = excluded[i] || sensed[i];
            numExcluded += excluded[i];
        }

        if (result.numCandidates - numExcluded >= minSurvivors)
            break;
        // Step 7: raise the threshold by 3 dB and repeat from step 4. No SL-RSRP
        // can realistically exceed 0 dBm, so this terminates even when the
        // exclusions are threshold-independent.
        if (threshold > 0)
            break;
        threshold += 3;
    }

    result.finalThresholdDbm = threshold;
    result.numSurvivors = result.numCandidates - numExcluded;

    if (result.numSurvivors <= 0) {
        // Degenerate pool: nothing survives even at the top threshold. The spec
        // has no case for this (step 5a and step 7 are meant to prevent it), so
        // fall back to a uniform pick over the whole window.
        int pick = random_->intuniform(0, result.numCandidates - 1);
        result.slot = windowStart + pick / positions;
        result.firstSubchannel = pick % positions;
    }
    else {
        // uniform pick among the survivors
        int pick = random_->intuniform(0, result.numSurvivors - 1);
        int seen = 0;
        for (int i = 0; i < (int)excluded.size(); i++) {
            if (excluded[i])
                continue;
            if (seen++ == pick) {
                result.slot = windowStart + i / positions;
                result.firstSubchannel = i % positions;
                break;
            }
        }
    }

    assert(result.slot != SLOTINDEX_NONE);
    return result;
}

int SlMode2Selector::drawReselectionCounter(int periodMs)
{
    // TS 38.321 §5.22.1.1: [5, 15] for a reservation interval of 100 ms or
    // more; otherwise [5*Q, 15*Q] with Q = ceil(100 / max(20, P_rsvp_TX)).
    // The max(20, ...) floor caps Q at 5 -- without it, sub-20 ms periods draw
    // counters that keep a resource far longer than the spec intends.
    int q = (periodMs >= 100) ? 1 : (int)std::ceil(100.0 / std::max(20, periodMs));
    return random_->intuniform(5 * q, 15 * q);
}

} // namespace simu5g
