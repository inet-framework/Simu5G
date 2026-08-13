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

namespace {

/// Q of TS 38.321 5.22.1.1: the reselection-counter interval is [5Q, 15Q].
int counterScale(int periodMs)
{
    return (periodMs >= 100) ? 1 : (int)std::ceil(100.0 / std::max(20, periodMs));
}

} // namespace

int SlMode2Selector::maxReselectionCounter(int periodMs)
{
    return 15 * counterScale(periodMs);
}

int SlMode2Selector::drawReselectionCounter(int periodMs)
{
    // TS 38.321 §5.22.1.1: [5, 15] for a reservation interval of 100 ms or
    // more; otherwise [5*Q, 15*Q] with Q = ceil(100 / max(20, P_rsvp_TX)).
    // The max(20, ...) floor caps Q at 5 -- without it, sub-20 ms periods draw
    // counters that keep a resource far longer than the spec intends.
    int q = counterScale(periodMs);
    return random_->intuniform(5 * q, 15 * q);
}

/**
 * The heart of step 6c, shared by steps 5 and 6.
 *
 * A candidate at slot y is unusable when the resource it anchors, or ANY of
 * the C_resel-1 repetitions that follow it at the transmitter's own
 * reservation period, overlaps an occurrence of a sensed reservation. So an
 * occurrence at slot s excludes not only the candidate at s (the j = 0 case)
 * but also the candidates at s - j*P_TX for j = 1..C_resel-1.
 *
 * Skipping the j > 0 shifts is not a small approximation: a reservation whose
 * period exceeds the selection window -- a 100 ms mode-1 configured-grant
 * train sensed by a 20 ms mode-2 selector with a 10-slot window, say -- has an
 * occurrence inside the window only about a tenth of the time, so it would be
 * invisible to most selections and could not protect itself at all.
 */
void SlMode2Selector::excludeOccurrence(std::vector<char>& excluded, SlotIndex windowStart, SlotIndex windowEnd,
        int lSubch, SlotIndex slot, int resFirst, int resNum, int txPeriodSlots, int cResel) const
{
    const int positions = pool_.numSubchannels - lSubch + 1;
    const int lo = std::max(0, resFirst - lSubch + 1);
    const int hi = std::min(positions - 1, resFirst + resNum - 1);

    auto excludeAt = [&](SlotIndex y) {
        if (y < windowStart || y > windowEnd)
            return;
        char *row = &excluded[(y - windowStart) * positions];
        for (int p = lo; p <= hi; p++)
            row[p] = 1;
    };

    excludeAt(slot);                       // j = 0
    if (txPeriodSlots <= 0)
        return;                            // one-shot selection: no train to protect
    // Start at the first j whose back-shift can still land inside the window.
    SlotIndex jFirst = (slot > windowEnd) ? (slot - windowEnd + txPeriodSlots - 1) / txPeriodSlots : 1;
    for (SlotIndex j = jFirst; j < cResel; j++) {
        SlotIndex y = slot - j * txPeriodSlots;
        if (y < windowStart)
            break;
        excludeAt(y);
    }
}

std::vector<char> SlMode2Selector::excludeUnmonitored(SlotIndex windowStart, SlotIndex windowEnd, int lSubch,
        const SlSensingDatabase& db, int txPeriodSlots, int cResel) const
{
    const int windowSlots = (int)(windowEnd - windowStart) + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    std::vector<char> excluded(windowSlots * positions, 0);

    const SlotIndex horizon = windowEnd + (txPeriodSlots > 0 ? (SlotIndex)(cResel - 1) * txPeriodSlots : 0);

    // TS 38.214 §8.1.4 step 5: for a slot the UE did not monitor, assume an SCI
    // was received there reserving ALL subchannels of the pool, at every period
    // the pool allows, and apply the step-6 condition to it.
    for (SlotIndex m : db.getUnmonitoredSlots()) {
        for (int periodSlots : pool_.allowedPeriodsSlots) {
            if (periodSlots <= 0)
                continue;
            for (SlotIndex s = m + periodSlots; s <= horizon; s += periodSlots)
                excludeOccurrence(excluded, windowStart, windowEnd, lSubch, s, 0, pool_.numSubchannels,
                        txPeriodSlots, cResel);
        }
    }
    return excluded;
}

std::vector<char> SlMode2Selector::excludeSensed(SlotIndex windowStart, SlotIndex windowEnd, int lSubch,
        const SlSensingDatabase& db, double thresholdDbm, int txPeriodSlots, int cResel) const
{
    const int windowSlots = (int)(windowEnd - windowStart) + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    std::vector<char> excluded(windowSlots * positions, 0);

    // Every occurrence that can still shift back onto an in-window candidate.
    // This walks each reservation's occurrences without the spec's q <= Q cap:
    // a conservative superset, and the cap is stated for the selection window
    // alone, which the own-repetition horizon extends past.
    const SlotIndex horizon = windowEnd + (txPeriodSlots > 0 ? (SlotIndex)(cResel - 1) * txPeriodSlots : 0);

    for (const auto& e : db.getEntries()) {
        if (e.rsrpDbm < thresholdDbm || e.reservationPeriodSlots <= 0)
            continue;
        for (SlotIndex s = e.slot + e.reservationPeriodSlots; s <= horizon; s += e.reservationPeriodSlots)
            excludeOccurrence(excluded, windowStart, windowEnd, lSubch, s, e.firstSubchannel, e.numSubchannels,
                    txPeriodSlots, cResel);
    }
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

    // C_resel for the exclusion horizon: the longest SPS train a selection can
    // anchor. Deliberately the upper bound of the counter draw rather than the
    // drawn value, so the exclusion is a function of the sensing state alone
    // and does not depend on when the counter happens to be drawn.
    const int cResel = maxReselectionCounter(periodMs);

    // Drawn here rather than after the pick purely to keep the RNG draw order
    // of the mode-2 branch: the exclusion above uses maxReselectionCounter and
    // does not depend on this value.
    result.reselectionCounter = drawReselectionCounter(periodMs);

    const int minSurvivors = (int)std::ceil(pool_.txPercentage * result.numCandidates);

    // Step 5, run once: it does not depend on the RSRP threshold.
    const std::vector<char> unmonitored = excludeUnmonitored(windowStart, windowEnd, lSubch, db, periodSlots, cResel);
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
        const std::vector<char> sensed = excludeSensed(windowStart, windowEnd, lSubch, db,
                threshold, periodSlots, cResel);
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

} // namespace simu5g
