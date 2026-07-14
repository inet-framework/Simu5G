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

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace simu5g {

std::vector<char> SlMode2Selector::computeExclusion(SlotIndex now, int lSubch, const SlSensingDatabase& db, double thresholdDbm, int& numExcluded) const
{
    const int windowSlots = pool_.t2 - pool_.t1 + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    const SlotIndex windowStart = now + pool_.t1;
    const SlotIndex windowEnd = now + pool_.t2;

    // candidate grid, slot-major: index = (slot - windowStart) * positions + firstSubchannel
    std::vector<char> excluded(windowSlots * positions, 0);

    auto excludeOverlapping = [&](SlotIndex slot, int resFirst, int resNum) {
        if (slot < windowStart || slot > windowEnd)
            return;
        // exclude candidates [first, first+lSubch-1] overlapping [resFirst, resFirst+resNum-1]
        int lo = std::max(0, resFirst - lSubch + 1);
        int hi = std::min(positions - 1, resFirst + resNum - 1);
        for (int p = lo; p <= hi; p++)
            excluded[(slot - windowStart) * positions + p] = 1;
    };

    // exclusion step 1: projected reservations of sensed SCIs above threshold
    for (const auto& e : db.getEntries()) {
        if (e.rsrpDbm < thresholdDbm || e.reservationPeriodSlots <= 0)
            continue;
        for (SlotIndex s = e.slot + e.reservationPeriodSlots; s <= windowEnd; s += e.reservationPeriodSlots)
            excludeOverlapping(s, e.firstSubchannel, e.numSubchannels);
    }

    // exclusion step 2 (conservative): whole slots that could carry a
    // reservation projected from an unmonitored (own-TX) slot
    for (SlotIndex m : db.getUnmonitoredSlots()) {
        for (int periodSlots : pool_.allowedPeriodsSlots) {
            if (periodSlots <= 0)
                continue;
            for (SlotIndex s = m + periodSlots; s <= windowEnd; s += periodSlots)
                excludeOverlapping(s, 0, pool_.numSubchannels);
        }
    }

    numExcluded = 0;
    for (char e : excluded)
        numExcluded += e;
    return excluded;
}

SlMode2Selector::Selection SlMode2Selector::select(SlotIndex now, int lSubch, int periodSlots, int periodMs, const SlSensingDatabase& db)
{
    if (lSubch < 1 || lSubch > pool_.numSubchannels)
        throw std::invalid_argument("SlMode2Selector: invalid L_subCH");
    if (pool_.t2 <= pool_.t1)
        throw std::invalid_argument("SlMode2Selector: invalid selection window");

    const int windowSlots = pool_.t2 - pool_.t1 + 1;
    const int positions = pool_.numSubchannels - lSubch + 1;
    const SlotIndex windowStart = now + pool_.t1;

    Selection result;
    result.numCandidates = windowSlots * positions;

    // the 20% rule: raise the threshold by 3 dB until at least 20% of the
    // candidates survive the exclusion
    double threshold = pool_.rsrpThresholdDbm;
    int numExcluded = 0;
    std::vector<char> excluded;
    while (true) {
        excluded = computeExclusion(now, lSubch, db, threshold, numExcluded);
        int survivors = result.numCandidates - numExcluded;
        if (survivors * 5 >= result.numCandidates)  // survivors >= 20% of M_total
            break;
        // note: step-2 (unmonitored-slot) exclusions are threshold-independent;
        // if they alone exceed 80% of the pool the loop must still terminate
        if (threshold > 0) {  // no SL-RSRP can realistically exceed 0 dBm
            break;
        }
        threshold += 3;
    }

    result.finalThresholdDbm = threshold;
    result.numSurvivors = result.numCandidates - numExcluded;

    if (result.numSurvivors <= 0) {
        // degenerate pool (e.g. everything blocked by half-duplex projections):
        // fall back to a uniform pick over all candidates, per the random
        // baseline (the spec's re-evaluation machinery is out of SL-1 scope)
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
    (void)periodSlots;
    result.reselectionCounter = drawReselectionCounter(periodMs);
    return result;
}

int SlMode2Selector::drawReselectionCounter(int periodMs)
{
    // TS 38.321 §5.22.1.1: [5,15] for periods >= 100 ms, scaled by
    // Q = ceil(100/period) below
    int q = (periodMs >= 100) ? 1 : (int)std::ceil(100.0 / std::max(periodMs, 1));
    return random_->intuniform(5 * q, 15 * q);
}

} // namespace simu5g
