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

#include "simu5g/stack/sidelink/mac/SlEnbScheduler.h"

#include <algorithm>

#include "simu5g/stack/sidelink/mac/SlMcsTable.h"

namespace simu5g {

int SlEnbScheduler::widthForBytes(int bytes) const
{
    // one full-width TB is the per-cycle cap: a dynamic grant carries one TB
    int cap = (int)SlMcsTable::tbsBytes(cfg_.mcs, cfg_.numSubchannels * cfg_.subchannelSize, cfg_.overheadSymbols);
    int target = std::min(bytes, cap);
    for (int width = 1; width <= cfg_.numSubchannels; width++) {
        int tbs = (int)SlMcsTable::tbsBytes(cfg_.mcs, width * cfg_.subchannelSize, cfg_.overheadSymbols);
        if (tbs >= target)
            return width;
    }
    return cfg_.numSubchannels;
}

bool SlEnbScheduler::isFree(SlotIndex slot, uint64_t m) const
{
    auto it = grid_.find(slot);
    return it == grid_.end() || (it->second & m) == 0;
}

void SlEnbScheduler::commit(SlotIndex slot, uint64_t m)
{
    grid_[slot] |= m;
}

void SlEnbScheduler::pruneBefore(SlotIndex slot)
{
    grid_.erase(grid_.begin(), grid_.lower_bound(slot));
}

SlEnbScheduler::GrantSpec SlEnbScheduler::onSlBsr(MacNodeId ueId, int reportedBytes, SlotIndex nowSlot)
{
    pruneBefore(nowSlot);

    GrantSpec spec;
    if (reportedBytes <= 0)
        return spec;

    int width = widthForBytes(reportedBytes);
    SlotIndex earliest = nowSlot + cfg_.ueProcessingSlots;

    // deterministic first fit: earliest occasion train, lowest subchannel
    // offset, whose numOccasions occasions are all free
    for (SlotIndex s = earliest; s <= earliest + cfg_.schedulingHorizonSlots; s++) {
        for (int f = 0; f + width <= cfg_.numSubchannels; f++) {
            uint64_t m = mask(f, width);
            bool free = true;
            for (int k = 0; k < cfg_.numOccasions && free; k++)
                free = isFree(s + (SlotIndex)k * cfg_.occasionGapSlots, m);
            if (!free)
                continue;

            for (int k = 0; k < cfg_.numOccasions; k++)
                commit(s + (SlotIndex)k * cfg_.occasionGapSlots, m);

            spec.firstSlot = s;
            spec.numOccasions = cfg_.numOccasions;
            spec.occasionGapSlots = cfg_.occasionGapSlots;
            spec.firstSubchannel = f;
            spec.numSubchannels = width;
            spec.mcs = cfg_.mcs;
            spec.tbBytes = (int)SlMcsTable::tbsBytes(cfg_.mcs, width * cfg_.subchannelSize, cfg_.overheadSymbols);
            return spec;
        }
    }

    // horizon exhausted: no grant (the UE's BSR machinery retries)
    return spec;
}

} // namespace simu5g
