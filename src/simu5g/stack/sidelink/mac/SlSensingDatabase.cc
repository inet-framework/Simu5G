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

#include "simu5g/stack/sidelink/mac/SlSensingDatabase.h"

namespace simu5g {

void SlSensingDatabase::recordSci(const SlSensingEntry& entry)
{
    entries_.push_back(entry);
    pruneBefore(entry.slot - sensingWindowSlots_);
}

void SlSensingDatabase::recordUnmonitoredSlot(SlotIndex slot)
{
    unmonitoredSlots_.push_back(slot);
    pruneBefore(slot - sensingWindowSlots_);
}

void SlSensingDatabase::pruneBefore(SlotIndex slot)
{
    while (!entries_.empty() && entries_.front().slot < slot)
        entries_.pop_front();
    while (!unmonitoredSlots_.empty() && unmonitoredSlots_.front() < slot)
        unmonitoredSlots_.pop_front();
}

} // namespace simu5g
