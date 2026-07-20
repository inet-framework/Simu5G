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
 *  - candidate resources: single-slot resources of lSubch contiguous
 *    subchannels in the selection window [n+T1, n+T2] (every slot belongs to
 *    the pool in SL-1: slotBitmap "all")
 *  - exclusion step 1: candidates overlapping *projected* reservations
 *    (reservation-period extrapolation of sensed SCIs) whose SL-RSRP is
 *    above the threshold
 *  - exclusion step 2 (conservative): candidates in slots that could carry a
 *    projected reservation from any allowed period landing on a slot the UE
 *    did not monitor (own half-duplex transmissions)
 *  - the 20% rule: while fewer than 20% of the candidates survive, raise the
 *    RSRP threshold by 3 dB and re-run the exclusion
 *  - uniform random pick among the survivors; reselection counter drawn from
 *    [5*Q, 15*Q], Q = ceil(100 / max(periodMs, 1)) for periods below 100 ms,
 *    Q = 1 otherwise (TS 38.321 §5.22.1.1)
 *
 * Explicitly out of SL-1 scope: re-evaluation and pre-emption checks
 * (Rel-16 TS 38.214 §8.1.4 re-evaluation), multi-slot/multi-resource
 * chains within one period, inter-UE coordination (Rel-17), priority-pair
 * (txPrio, rxPrio) threshold tables (a single configured threshold is used).
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
    };

    struct Selection {
        SlotIndex slot = SLOTINDEX_NONE;   // selected TX slot (absolute)
        int firstSubchannel = 0;
        int reselectionCounter = 0;
        // diagnostics (also exercised by the unit tests)
        double finalThresholdDbm = 0;      // threshold after 20%-rule iterations
        int numCandidates = 0;             // M_total
        int numSurvivors = 0;              // candidates left after exclusion
    };

  private:
    PoolConfig pool_;
    ISlRandom *random_;

    /// run both exclusion steps at the given threshold; returns the exclusion
    /// bitmap over the candidate grid (slot-major)
    std::vector<char> computeExclusion(SlotIndex now, int lSubch, int ownPeriodSlots, int cResel,
            const SlSensingDatabase& db, double thresholdDbm, int& numExcluded) const;

  public:
    SlMode2Selector(const PoolConfig& pool, ISlRandom *random) : pool_(pool), random_(random) {}

    const PoolConfig& getPool() const { return pool_; }

    /// TS 38.321 §5.22.1.1: select a periodic resource of lSubch subchannels
    /// for a flow with the given reservation period
    Selection select(SlotIndex now, int lSubch, int periodSlots, int periodMs, const SlSensingDatabase& db);

    /// draw a fresh reselection counter (also used by probResourceKeep)
    int drawReselectionCounter(int periodMs);

    /// upper bound of the drawReselectionCounter draw for a period: the
    /// longest SPS train a selection can anchor (the step-6 j horizon)
    static int maxReselectionCounter(int periodMs);
};

} // namespace simu5g

#endif
