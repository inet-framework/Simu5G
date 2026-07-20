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

#ifndef _SIDELINK_SLENBSCHEDULER_H_
#define _SIDELINK_SLENBSCHEDULER_H_

#include <cstdint>
#include <map>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * gNB-side mode-1 sidelink resource allocator (design decision D29, SL-3):
 * a free-standing pure class owned by SlGnbRrc - the SL pool is NOT a gNB
 * component carrier, so this allocator deliberately lives outside the Uu
 * scheduler machinery (seam 17). Plain C++, no cModule dependency,
 * unit-testable (D13).
 *
 * State: the pool geometry, an allocation grid of *future* commitments
 * (slot -> subchannel bitmask, pruned lazily on allocation - the D9 idiom,
 * no ticker), and the configured-grant registry (WP-P). Event-driven: BSRs
 * arrive one at a time via onSlBsr(), so allocation is deterministic
 * first-fit in arrival order (no tie-breaks to randomize).
 *
 * A dynamic grant is a finite occasion train (D30): numOccasions occasions
 * spaced occasionGapSlots apart, all on the same subchannel range, the
 * first at least ueProcessingSlots after `nowSlot` (D28). The TB geometry
 * is the smallest subchannel count whose TBS at the configured MCS covers
 * min(reported bytes, one-full-width TB) - one TB per grant cycle, the
 * honest latency cost of dynamic mode 1 (configured grants are the
 * throughput answer).
 */
class SlEnbScheduler
{
  public:
    struct Config
    {
        int numSubchannels = 5;
        int subchannelSize = 10;        // PRBs per subchannel
        unsigned int mcs = 6;           // PSSCH MCS the grants carry (gNB-configured, D29)
        int overheadSymbols = 5;        // TBS math, must match the UE's setting
        int ueProcessingSlots = 3;      // min gap between grant issue and first occasion (D28)
        int numOccasions = 3;           // per dynamic grant: 1 initial + preallocated retx (D30)
        int occasionGapSlots = 4;       // spacing of a dynamic grant's occasions
        int schedulingHorizonSlots = 256;  // give up if no free train within this window
    };

    struct GrantSpec
    {
        SlotIndex firstSlot = SLOTINDEX_NONE;
        int periodSlots = 0;            // CG: standing period (0 = dynamic)
        int numOccasions = 0;
        int occasionGapSlots = 0;
        int firstSubchannel = 0;
        int numSubchannels = 0;
        unsigned int mcs = 0;
        int tbBytes = 0;

        bool isValid() const { return firstSlot != SLOTINDEX_NONE; }
    };

    /// a standing configured-grant reservation (D30, WP-P): the periodic
    /// train {offsetSlot + k*periodSlots} on a fixed subchannel range,
    /// reserved while configured (type 2 stays reserved even while dormant -
    /// documented simplification vs. the spec's release)
    struct CgReservation
    {
        MacNodeId ueId;
        SlotIndex offsetSlot = 0;
        int periodSlots = 1;
        uint64_t mask = 0;
    };

  private:
    Config cfg_;

    // future dynamic commitments: slot -> bitmask of committed subchannels
    std::map<SlotIndex, uint64_t> grid_;

    // standing CG reservations, never pruned
    std::vector<CgReservation> cgs_;

    /// subchannel-mask of all CG trains that own the given slot
    uint64_t cgMaskAt(SlotIndex slot) const;

    /// would the periodic train {offset + k*period} (k >= 0) ever collide
    /// with an existing CG train on an overlapping subchannel range?
    /// Two arithmetic progressions {a + i*p}, {b + j*q} intersect (over all
    /// i,j >= 0 beyond the later start) iff (b - a) mod gcd(p, q) == 0 -
    /// exact, and equivalent to a hyper-period scan
    bool cgTrainCollides(SlotIndex offset, int periodSlots, uint64_t m) const;

    /// smallest subchannel count whose TBS covers the target (clamped to full width)
    int widthForBytes(int bytes) const;

    /// occupied-subchannel mask [firstSubchannel, firstSubchannel+width)
    static uint64_t mask(int firstSubchannel, int width)
    {
        return ((UINT64_C(1) << width) - 1) << firstSubchannel;
    }

    bool isFree(SlotIndex slot, uint64_t m) const;
    void commit(SlotIndex slot, uint64_t m);
    void pruneBefore(SlotIndex slot);

  public:
    explicit SlEnbScheduler(const Config& cfg) : cfg_(cfg) {}

    const Config& getConfig() const { return cfg_; }

    /**
     * Allocate a dynamic grant for a UE reporting `reportedBytes` of SL
     * backlog. Returns an invalid spec when no free occasion train exists
     * within the scheduling horizon (the UE retries via its BSR machinery).
     */
    GrantSpec onSlBsr(MacNodeId ueId, int reportedBytes, SlotIndex nowSlot);

    /**
     * Reserve a standing configured-grant train (D30): period periodSlots,
     * TB geometry sized for tbBytes, first occasion within one period of
     * nowSlot + ueProcessingSlots. The train is free of all existing CG
     * trains (gcd intersection test) and all current dynamic commitments;
     * subsequent dynamic allocations avoid it. Invalid spec = pool full.
     */
    GrantSpec reserveConfiguredGrant(MacNodeId ueId, int periodSlots, int tbBytes, SlotIndex nowSlot);

    /// number of future slots currently carrying commitments (introspection/tests)
    size_t committedSlotCount() const { return grid_.size(); }

    /// number of standing CG reservations (introspection/tests)
    size_t cgCount() const { return cgs_.size(); }
};

} // namespace simu5g

#endif
