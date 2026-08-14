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

#ifndef _SIDELINK_SLCRTRACKER_H_
#define _SIDELINK_SLCRTRACKER_H_

#include <deque>
#include <utility>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * Own channel-occupancy-ratio (CR) tracking (design decision D22, a TS
 * 38.214 8.1.7 abstraction): CR = own transmitted subchannel-slots within
 * the trailing window, over the window's total (slots x pool subchannels).
 * A pure counter on the grant train -- no per-slot ticker; entries are
 * pruned lazily. Plain C++ class (D13, unit-tested).
 */
class SlCrTracker
{
    std::deque<std::pair<SlotIndex, int>> transmissions_;   // (slot, subchannels used)

  public:
    void recordTx(SlotIndex slot, int numSubchannels)
    {
        transmissions_.emplace_back(slot, numSubchannels);
    }

    /// CR over (now - windowSlots, now]
    double cr(SlotIndex now, int windowSlots, int poolNumSubchannels)
    {
        while (!transmissions_.empty() && transmissions_.front().first <= now - windowSlots)
            transmissions_.pop_front();
        long used = 0;
        for (const auto& [slot, subchannels] : transmissions_)
            if (slot <= now)  // future reservations are not modeled
                used += subchannels;
        return (double)used / ((double)windowSlots * poolNumSubchannels);
    }
};

} // namespace simu5g

#endif
