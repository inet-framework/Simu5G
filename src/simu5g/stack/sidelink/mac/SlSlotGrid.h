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

#ifndef _SIDELINK_SLSLOTGRID_H_
#define _SIDELINK_SLSLOTGRID_H_

#include <omnetpp.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * Pure slot/numerology arithmetic on the SL carrier's grid (no cModule
 * dependency, unit-testable). Slot 0 starts at simtime 0; slot k covers
 * [k*slotDuration, (k+1)*slotDuration).
 */
class SlSlotGrid
{
    omnetpp::simtime_t slotDuration_;

  public:
    SlSlotGrid() : slotDuration_(omnetpp::SimTime(1, omnetpp::SIMTIME_MS)) {}
    explicit SlSlotGrid(omnetpp::simtime_t slotDuration) : slotDuration_(slotDuration) {}

    omnetpp::simtime_t getSlotDuration() const { return slotDuration_; }

    /// index of the slot containing time t
    SlotIndex slotIndexAt(omnetpp::simtime_t t) const
    {
        return (SlotIndex)(t.raw() / slotDuration_.raw());
    }

    /// start time of a slot
    omnetpp::simtime_t slotStart(SlotIndex slot) const
    {
        return omnetpp::SimTime::fromRaw(slot * slotDuration_.raw());
    }

    /// first slot of a periodic grant train {firstSlot + k*periodSlots} that is
    /// strictly after the given slot
    SlotIndex nextOccasionAfter(SlotIndex slot, SlotIndex firstSlot, int periodSlots) const
    {
        ASSERT(periodSlots > 0);
        if (slot < firstSlot)
            return firstSlot;
        SlotIndex k = (slot - firstSlot) / periodSlots + 1;
        return firstSlot + k * periodSlots;
    }

    /// number of whole slots in a millisecond-denominated period
    int slotsPerMs(int ms) const
    {
        return (int)(omnetpp::SimTime(ms, omnetpp::SIMTIME_MS).raw() / slotDuration_.raw());
    }
};

} // namespace simu5g

#endif
