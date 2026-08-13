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

#ifndef _SIDELINK_SLMODE2SELECTOR_H_
#define _SIDELINK_SLMODE2SELECTOR_H_

#include <vector>

#include "simu5g/stack/sidelink/mac/SlSensingDatabase.h"

namespace simu5g {

/**
 * Randomness injection seam (D13): the MAC provides an adapter over its
 * module RNG; unit tests provide a deterministic implementation.
 */
class ISlRandom
{
  public:
    virtual ~ISlRandom() {}
    virtual double uniform01() = 0;                 // uniform in [0,1)
    virtual int intuniform(int a, int b) = 0;       // uniform integer in [a,b]
};

/**
 * Mode-2 (UE-autonomous) sensing-based resource selection, TS 38.321
 * §5.22.1.1 / TS 38.214 §8.1.4, faithful on the modeled subset:
 *
 * The steps are numbered as in TS 38.214 §8.1.4:
 *
 *  - step 1: candidate resources are the single-slot resources of lSubch
 *    contiguous subchannels in the selection window [n+T1, n+T2]. T1 is
 *    capped at T_proc,1(mu) (Table 8.1.4-2). Every slot belongs to the pool
 *    (slotBitmap "all").
 *  - step 2: the sensing window is [n-T0, n-T_proc,0(mu)) (Table 8.1.4-1);
 *    slots the UE did not monitor (its own transmissions) are recorded.
 *  - step 4: S_A is initialized to all candidates.
 *  - step 5: exclude candidates that a hypothetical SCI, received in an
 *    unmonitored slot and reserving every subchannel, could collide with.
 *  - step 5a: if fewer than X*M_total candidates remain, S_A is reset to all
 *    candidates -- the unmonitored-slot exclusion is dropped rather than
 *    allowed to starve the selection.
 *  - step 6: exclude candidates whose resource, or any of the C_resel-1
 *    repetitions that follow it at the transmitter's own reservation period,
 *    overlaps a sensed reservation above the RSRP threshold, projected over
 *    q = 1..Q occasions of the sender's period.
 *  - step 7: while fewer than X*M_total candidates survive, raise the RSRP
 *    threshold by 3 dB and repeat from step 4.
 *
 * The surviving set is picked from uniformly. The reselection counter is
 * drawn from [5*Q, 15*Q] with Q = ceil(100 / max(20, periodMs)) below 100 ms
 * and Q = 1 at or above it (TS 38.321 §5.22.1.1).
 *
 * Modeled subset -- explicitly not implemented: re-evaluation and pre-emption
 * checks (TS 38.214 §8.1.4 / TS 38.321 §5.22.1.2a), multi-slot resources
 * within one period, inter-UE coordination (Rel-17), and the per-priority-pair
 * threshold table sl-Thres-RSRP-List (a single configured threshold stands in,
 * so L1 priority does not yet influence exclusion). T2 is taken from the pool
 * configuration rather than from the remaining packet delay budget.
 *
 * Plain C++ class (no cModule/omnetpp dependency, D13): "now" is a slot
 * index, randomness is injected via ISlRandom.
 */
class SlMode2Selector
{
  public:
    struct PoolConfig {
        int numSubchannels = 5;
        int t1 = 2;                        // selection window start offset [slots]
        int t2 = 20;                       // selection window end offset [slots]
        double rsrpThresholdDbm = -128;    // initial exclusion threshold
        std::vector<int> allowedPeriodsSlots;  // for the conservative unmonitored-slot exclusion
        int numerology = 1;                // mu of the SL BWP; picks the T_proc rows
        double txPercentage = 0.2;         // X: sl-TxPercentageList, as a ratio
    };

    /// TS 38.214 Table 8.1.4-1: sensing must stop this many slots before n.
    static int tProc0Slots(int numerology);
    /// TS 38.214 Table 8.1.4-2: the earliest selectable slot is n + this.
    static int tProc1Slots(int numerology);

    struct Selection {
        SlotIndex slot = SLOTINDEX_NONE;   // selected TX slot (absolute)
        int firstSubchannel = 0;
        int reselectionCounter = 0;
        // diagnostics (also exercised by the unit tests)
        double finalThresholdDbm = 0;      // threshold after the step-7 iterations
        int numCandidates = 0;             // M_total
        int numSurvivors = 0;              // candidates left after exclusion
        bool step5aApplied = false;        // the unmonitored-slot exclusion was dropped
    };

  private:
    PoolConfig pool_;
    ISlRandom *random_;

    /// Step 5: candidates a hypothetical SCI in an unmonitored slot could hit.
    std::vector<char> excludeUnmonitored(SlotIndex windowStart, SlotIndex windowEnd, int lSubch, const SlSensingDatabase& db) const;

    /// Step 6: candidates colliding with a sensed reservation above the
    /// threshold, counting the transmitter's own cResel repetitions.
    std::vector<char> excludeSensed(SlotIndex now, SlotIndex windowStart, SlotIndex windowEnd, int lSubch,
            const SlSensingDatabase& db, double thresholdDbm, int txPeriodSlots, int cResel) const;

  public:
    SlMode2Selector(const PoolConfig& pool, ISlRandom *random) : pool_(pool), random_(random) {}

    const PoolConfig& getPool() const { return pool_; }

    /// TS 38.321 §5.22.1.1: select a periodic resource of lSubch subchannels
    /// for a flow with the given reservation period
    Selection select(SlotIndex now, int lSubch, int periodSlots, int periodMs, const SlSensingDatabase& db);

    /// draw a fresh reselection counter (also used by probResourceKeep)
    int drawReselectionCounter(int periodMs);
};

} // namespace simu5g

#endif
