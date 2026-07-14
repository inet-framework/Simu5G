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

//
// Deterministic unit tests of the sidelink algorithm classes (design decision
// D13): SlSlotGrid, SlSensingDatabase and SlMode2Selector are plain C++
// classes with injected randomness, exercised here against hand-computed
// expectations. Run via ./runtest; any failed check aborts the simulation
// with a nonzero exit code.
//

#include <omnetpp.h>

#include "simu5g/stack/sidelink/mac/SlMode2Selector.h"
#include "simu5g/stack/sidelink/mac/SlSensingDatabase.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

using namespace omnetpp;

namespace simu5g {

/// deterministic ISlRandom: intuniform returns lo + fixed (clamped to hi)
class FixedRandom : public ISlRandom
{
  public:
    int fixed = 0;
    double uniform01() override { return 0.0; }
    int intuniform(int a, int b) override { return std::min(a + fixed, b); }
};

/// tiny deterministic LCG for bounds checks
class LcgRandom : public ISlRandom
{
    uint32_t state_ = 12345;
    uint32_t next() { state_ = state_ * 1664525u + 1013904223u; return state_; }
  public:
    double uniform01() override { return next() / 4294967296.0; }
    int intuniform(int a, int b) override { return a + (int)(next() % (uint32_t)(b - a + 1)); }
};

class SlAlgorithmTest : public cSimpleModule
{
  protected:
    int numChecks_ = 0;

    void check(bool condition, const char *what)
    {
        numChecks_++;
        if (!condition)
            throw cRuntimeError("CHECK FAILED: %s", what);
        EV << "ok: " << what << endl;
    }

    void initialize() override
    {
        testSlotGrid();
        testSensingDatabase();
        testSelectorExclusion();
        testTwentyPercentRule();
        testUnmonitoredExclusion();
        testDegeneratePoolFallback();
        testReselectionCounter();

        std::cout << "SlAlgorithmTest: ALL " << numChecks_ << " CHECKS PASSED" << std::endl;
    }

    void testSlotGrid()
    {
        SlSlotGrid grid(SimTime(500, SIMTIME_US));  // numerology 1
        check(grid.slotIndexAt(SimTime::ZERO) == 0, "slotGrid: slot 0 at t=0");
        check(grid.slotIndexAt(SimTime(500, SIMTIME_US)) == 1, "slotGrid: slot 1 at t=0.5ms");
        check(grid.slotIndexAt(SimTime(1250, SIMTIME_US)) == 2, "slotGrid: slot 2 at t=1.25ms");
        check(grid.slotStart(4) == SimTime(2, SIMTIME_MS), "slotGrid: slot 4 starts at 2ms");
        check(grid.slotsPerMs(20) == 40, "slotGrid: 20ms = 40 slots");
        check(grid.nextOccasionAfter(10, 0, 4) == 12, "slotGrid: next occasion of {0+4k} after 10 is 12");
        check(grid.nextOccasionAfter(12, 0, 4) == 16, "slotGrid: occasion at 'now' is skipped (strictly after)");
        check(grid.nextOccasionAfter(3, 5, 4) == 5, "slotGrid: first slot when now precedes the train");
    }

    void testSensingDatabase()
    {
        SlSensingDatabase db(10);
        for (SlotIndex s = 1; s <= 5; s++)
            db.recordSci(SlSensingEntry{s, 0, 1, -90, 20, MacNodeId(2049), 0});
        check(db.size() == 5, "sensingDb: five entries recorded");

        db.recordSci(SlSensingEntry{20, 0, 1, -90, 20, MacNodeId(2049), 0});
        check(db.size() == 1, "sensingDb: entries older than the window pruned on insert");
        check(db.getEntries().front().slot == 20, "sensingDb: newest entry kept");

        db.recordUnmonitoredSlot(21);
        db.recordUnmonitoredSlot(35);
        check(db.getUnmonitoredSlots().size() == 1, "sensingDb: unmonitored slots pruned by the same window");
        check(db.getUnmonitoredSlots().front() == 35, "sensingDb: newest unmonitored slot kept");
    }

    void testSelectorExclusion()
    {
        // pool: 5 subchannels, window [now+2, now+11]; L_subCH=2 -> 4 positions
        // per slot, M_total = 40
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 5;
        pool.t1 = 2;
        pool.t2 = 11;
        pool.rsrpThresholdDbm = -128;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        // one sensed SCI: slot 95, subchannels [1,2], period 10 slots, -90 dBm;
        // from now=100 it projects into the window only at slot 105
        SlSensingDatabase db(1600);
        db.recordSci(SlSensingEntry{95, 1, 2, -90, 10, MacNodeId(2049), 0});

        auto sel = selector.select(100, 2, 40, 20, db);
        check(sel.numCandidates == 40, "selector: M_total = 40");
        // in slot 105, positions 0..2 overlap subchannels [1,2] -> 3 excluded
        check(sel.numSurvivors == 37, "selector: 3 candidates excluded by the projected reservation");
        check(sel.finalThresholdDbm == -128, "selector: threshold untouched when >20% survive");

        // pick 0 -> first surviving candidate: window start, position 0
        check(sel.slot == 102 && sel.firstSubchannel == 0, "selector: pick 0 lands on the first candidate");

        // pick 12 must skip the 3 excluded candidates of slot 105 (indices
        // 12..14 in the slot-major grid) and land on (105, position 3)
        rng.fixed = 12;
        sel = selector.select(100, 2, 40, 20, db);
        check(sel.slot == 105 && sel.firstSubchannel == 3, "selector: pick skips excluded candidates");

        // the same SCI below the threshold excludes nothing
        SlSensingDatabase quietDb(1600);
        quietDb.recordSci(SlSensingEntry{95, 1, 2, -129, 10, MacNodeId(2049), 0});
        rng.fixed = 0;
        sel = selector.select(100, 2, 40, 20, quietDb);
        check(sel.numSurvivors == 40, "selector: SCI below threshold is ignored");
    }

    void testTwentyPercentRule()
    {
        // 1 subchannel, window [101,110], all 10 slots covered by projected
        // reservations at -90 dBm -> the threshold must climb from -128 by
        // 3 dB steps until it exceeds -90 (13 steps -> -89)
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 1;
        pool.t1 = 1;
        pool.t2 = 10;
        pool.rsrpThresholdDbm = -128;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        for (SlotIndex s = 91; s <= 100; s++)
            db.recordSci(SlSensingEntry{s, 0, 1, -90, 10, MacNodeId(2049), 0});

        auto sel = selector.select(100, 1, 200, 100, db);
        check(sel.numCandidates == 10, "20% rule: M_total = 10");
        check(sel.finalThresholdDbm == -89, "20% rule: threshold raised 13x3dB to -89");
        check(sel.numSurvivors == 10, "20% rule: all candidates recovered above the final threshold");
    }

    void testUnmonitoredExclusion()
    {
        // unmonitored slot 100 with allowed period 5 projects to slots 105 and
        // 110 inside the window [102,111]: both are conservatively excluded in
        // full (5 positions each)
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 5;
        pool.t1 = 2;
        pool.t2 = 11;
        pool.rsrpThresholdDbm = -128;
        pool.allowedPeriodsSlots = { 5 };
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        db.recordUnmonitoredSlot(100);

        auto sel = selector.select(100, 1, 40, 20, db);
        check(sel.numCandidates == 50, "unmonitored: M_total = 50");
        check(sel.numSurvivors == 40, "unmonitored: two whole slots conservatively excluded");
    }

    void testDegeneratePoolFallback()
    {
        // allowed period 1 + an unmonitored slot right before the window
        // blocks every candidate slot; threshold iterations cannot help
        // (step-2 exclusion is threshold-independent) -> uniform fallback
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 2;
        pool.t1 = 2;
        pool.t2 = 6;
        pool.rsrpThresholdDbm = -128;
        pool.allowedPeriodsSlots = { 1 };
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        db.recordUnmonitoredSlot(101);

        auto sel = selector.select(100, 1, 40, 20, db);
        check(sel.numSurvivors == 0, "fallback: every candidate excluded");
        check(sel.slot >= 102 && sel.slot <= 106, "fallback: uniform pick still inside the window");
    }

    void testReselectionCounter()
    {
        SlMode2Selector::PoolConfig pool;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        rng.fixed = 0;
        check(selector.drawReselectionCounter(100) == 5, "counter: period >= 100ms draws from [5,15]");
        check(selector.drawReselectionCounter(20) == 25, "counter: period 20ms draws from [25,75] (Q=5)");

        LcgRandom lcg;
        SlMode2Selector selector2(pool, &lcg);
        bool inBounds = true;
        for (int i = 0; i < 200; i++) {
            int c = selector2.drawReselectionCounter(20);
            inBounds = inBounds && c >= 25 && c <= 75;
        }
        check(inBounds, "counter: 200 LCG draws stay within [25,75] for period 20ms");
    }
};

Define_Module(SlAlgorithmTest);

} // namespace simu5g
