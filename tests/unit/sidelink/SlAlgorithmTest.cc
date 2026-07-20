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

#include <cmath>

#include <omnetpp.h>

#include "simu5g/stack/sidelink/common/SlPreconfig.h"
#include "simu5g/stack/sidelink/common/SlPsfch.h"
#include "simu5g/stack/sidelink/mac/SlCrTracker.h"
#include "simu5g/stack/sidelink/mac/SlMcsTable.h"
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
        testMcsTable();
        testPsfchMath();
        testCbrLevelsAndCr();
        testGrantOccasionTrains();

        std::cout << "SlAlgorithmTest: ALL " << numChecks_ << " CHECKS PASSED" << std::endl;
    }

    void testGrantOccasionTrains()
    {
        // unbounded periodic train (numOccasions = 0): pre-SL-3 semantics
        SlGrant g;
        g.firstSlot = 10;
        g.periodSlots = 4;
        check(g.nextOccasionAfter(3) == 10, "grant: unbounded train starts at firstSlot");
        check(g.nextOccasionAfter(10) == 14, "grant: occasion at 'now' skipped (strictly after)");
        check(g.nextOccasionAfter(21) == 22, "grant: mid-train arithmetic");
        check(!g.isLastOccasion(10) && !g.isLastOccasion(1000), "grant: unbounded train has no last occasion");

        // finite mode-1 train (D30): occasions at firstSlot + k*gap, k < numOccasions
        SlGrant m;
        m.firstSlot = 100;
        m.numOccasions = 3;
        m.occasionGapSlots = 8;
        check(m.nextOccasionAfter(50) == 100, "grant: finite train first occasion");
        check(m.nextOccasionAfter(100) == 108, "grant: finite train second occasion");
        check(m.nextOccasionAfter(109) == 116, "grant: finite train third occasion");
        check(m.nextOccasionAfter(116) == SLOTINDEX_NONE, "grant: finite train exhausted after the last occasion");
        check(!m.isLastOccasion(100) && !m.isLastOccasion(108), "grant: non-final occasions are not last");
        check(m.isLastOccasion(116), "grant: final occasion detected");
        check(!m.isLastOccasion(117), "grant: non-occasion slot is not last");

        // single-occasion dynamic grant
        SlGrant s;
        s.firstSlot = 42;
        s.numOccasions = 1;
        check(s.nextOccasionAfter(0) == 42, "grant: single-occasion train");
        check(s.isLastOccasion(42), "grant: single occasion is the last");
        check(s.nextOccasionAfter(42) == SLOTINDEX_NONE, "grant: single-occasion train exhausted");
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

    void testMcsTable()
    {
        // table entries (TS 38.214 5.1.3.1-1 spot checks)
        check(SlMcsTable::entry(0).qm == 2 && SlMcsTable::entry(0).codeRateX1024 == 120.0,
              "mcs: entry 0 is QPSK 120/1024");
        check(SlMcsTable::entry(10).qm == 4 && SlMcsTable::entry(10).codeRateX1024 == 340.0,
              "mcs: entry 10 is 16QAM 340/1024");
        check(SlMcsTable::entry(28).qm == 6 && SlMcsTable::entry(28).codeRateX1024 == 948.0,
              "mcs: entry 28 is 64QAM 948/1024");

        // TBS, hand-computed per TS 38.214 5.1.3.2 with 9 data symbols
        // (overheadSymbols=5): RE/PRB = 108
        // mcs 0, 1 PRB: nInfo = 108*2*(120/1024) = 25.31 -> N'info = 24 -> 24 bits
        check(SlMcsTable::tbsBits(0, 1, 5) == 24, "mcs: TBS(mcs0, 1 PRB) = 24 bits");
        // mcs 6, 10 PRB: nInfo = 1080*2*(449/1024) = 947.1 -> n=3, N'info=944
        //  -> next table value 984 bits
        check(SlMcsTable::tbsBits(6, 10, 5) == 984, "mcs: TBS(mcs6, 10 PRB) = 984 bits");
        check(SlMcsTable::tbsBytes(6, 10, 5) == 123, "mcs: TBS(mcs6, 10 PRB) = 123 bytes");
        // mcs 15, 20 PRB: nInfo = 2160*4*(616/1024) = 5197.5 -> n=7,
        //  N'info = 128*round(5173/128) = 5120; R>1/4, N'info<8424
        //  -> TBS = 8*ceil(5144/8)-24 = 5120 bits
        check(SlMcsTable::tbsBits(15, 20, 5) == 5120, "mcs: TBS(mcs15, 20 PRB) = 5120 bits");
        // mcs 28, 50 PRB: nInfo = 5400*6*(948/1024) = 29995.3 -> n=9,
        //  N'info = 512*round(29971/512) = 30208; C = ceil(30232/8424) = 4
        //  -> TBS = 32*ceil(30232/32)-24 = 30216 bits (real-valued C, not the
        //  integer-division shortcut)
        check(SlMcsTable::tbsBits(28, 50, 5) == 30216, "mcs: TBS(mcs28, 50 PRB) = 30216 bits");
        // RE/PRB is capped at 156: zero overhead would give 168
        check(SlMcsTable::tbsBits(0, 1, 0) == SlMcsTable::computeTbsFromNinfo(
                  std::floor(156 * 2 * (120.0 / 1024.0)), 120.0 / 1024.0),
              "mcs: RE/PRB capped at 156");
        check(SlMcsTable::computeTbsFromNinfo(0, 0.5) == 0, "mcs: nInfo=0 -> TBS 0");

        // CQI map: largest CQI of 7.2.3-1 with efficiency <= the MCS's
        check(SlMcsTable::cqi(0) == 2, "mcs: cqi(0) = 2");
        check(SlMcsTable::cqi(9) == 6 && SlMcsTable::cqi(10) == 6, "mcs: cqi(9) = cqi(10) = 6");
        check(SlMcsTable::cqi(11) == 7, "mcs: cqi(11) = 7");
        check(SlMcsTable::cqi(17) == 9, "mcs: cqi(17) = 9 (64QAM 438 below CQI10's efficiency)");
        check(SlMcsTable::cqi(28) == 15, "mcs: cqi(28) = 15");
    }

    void testPsfchMath()
    {
        // feedback slot: first slot >= pssch + gap with slot % period == 0
        check(slPsfchFeedbackSlot(10, 4, 2) == 12, "psfch: slot 10, period 4, gap 2 -> 12");
        check(slPsfchFeedbackSlot(11, 4, 2) == 16, "psfch: slot 11, period 4, gap 2 -> 16 (13 rounds up)");
        check(slPsfchFeedbackSlot(12, 4, 2) == 16, "psfch: slot 12, period 4, gap 2 -> 16 (14 rounds up)");
        check(slPsfchFeedbackSlot(14, 4, 2) == 16, "psfch: slot 14, period 4, gap 2 -> 16 (exact)");
        check(slPsfchFeedbackSlot(10, 1, 2) == 12, "psfch: period 1 -> every slot is a PSFCH slot");
        check(slPsfchFeedbackSlot(9, 2, 2) == 12, "psfch: slot 9, period 2, gap 2 -> 12");
        check(slPsfchFeedbackSlot(10, 2, 0) == 10, "psfch: gap 0, aligned slot acks itself");
        check(slPsfchFeedbackSlot(10, 4, 2) - 10 >= 2
              && slPsfchFeedbackSlot(11, 4, 2) - 11 >= 2, "psfch: gap is always respected");

        // resource index: derived from the acknowledged PSSCH's (slot, subchannel)
        check(slPsfchResourceIndex(10, 3, 5, 0, 8) == (10 * 5 + 3) % 8, "psfch: resource index formula");
        // two PSSCHs in the same slot on different subchannels -> different resources
        check(slPsfchResourceIndex(10, 0, 5, 0, 8) != slPsfchResourceIndex(10, 1, 5, 0, 8),
              "psfch: co-slot PSSCHs on different subchannels get different resources");
        // deliberate collision: indices wrap modulo psfchResources
        check(slPsfchResourceIndex(10, 0, 5, 0, 8) == slPsfchResourceIndex(10, 0, 5, 8, 8),
              "psfch: member offset wraps modulo the resource count (collision case)");
        // groupcast option 2: distinct members -> distinct resources (within the modulus)
        check(slPsfchResourceIndex(20, 2, 5, 1, 8) == (20 * 5 + 2 + 1) % 8,
              "psfch: member index shifts the resource");
    }

    void testCbrLevelsAndCr()
    {
        // CBR level lookup: ascending cbrUpper thresholds, sticky top level
        SlPreconfig cfg;
        check(cfg.findCbrLevel(0.5) == nullptr, "cbr: empty table -> no level");
        SlCbrLevel l1; l1.cbrUpper = 0.3; l1.maxMcs = 16; l1.crLimit = 0.02;
        SlCbrLevel l2; l2.cbrUpper = 0.65; l2.maxMcs = 12; l2.crLimit = 0.01;
        SlCbrLevel l3; l3.cbrUpper = 1.0; l3.maxMcs = 9; l3.crLimit = 0.005;
        cfg.cbrConfig = { l1, l2, l3 };
        check(cfg.findCbrLevel(0.0)->maxMcs == 16, "cbr: CBR 0 -> first level");
        check(cfg.findCbrLevel(0.3)->maxMcs == 16, "cbr: boundary is inclusive");
        check(cfg.findCbrLevel(0.31)->maxMcs == 12, "cbr: mid level");
        check(cfg.findCbrLevel(0.9)->crLimit == 0.005, "cbr: top level");

        // CR window: subchannel-slots over (now - window, now]
        SlCrTracker cr;
        check(cr.cr(100, 1000, 5) == 0.0, "cr: empty tracker -> 0");
        cr.recordTx(100, 2);
        cr.recordTx(150, 1);
        check(cr.cr(150, 1000, 5) == 3.0 / 5000, "cr: 3 subchannel-slots in the window");
        // the slot exactly window-edge old falls out
        check(cr.cr(1100, 1000, 5) == 1.0 / 5000, "cr: slot 100 pruned at now=1100 (edge exclusive)");
        check(cr.cr(1150, 1000, 5) == 0.0, "cr: all pruned once the window passes");
    }
};

Define_Module(SlAlgorithmTest);

} // namespace simu5g
