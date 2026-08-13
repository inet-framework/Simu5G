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
#include <numeric>

#include <omnetpp.h>

#include "simu5g/stack/sidelink/common/SlPreconfig.h"
#include "simu5g/stack/sidelink/common/SlPsfch.h"
#include "simu5g/stack/sidelink/mac/SlCrTracker.h"
#include "simu5g/stack/sidelink/common/SlUeRadioState.h"
#include "simu5g/stack/sidelink/ip2nic/SlPathPolicy.h"
#include "simu5g/stack/sidelink/mac/SlEnbScheduler.h"
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
        testStep5aReset();
        testProcessingDelays();
        testTxPercentage();
        testReselectionCounter();
        testMcsTable();
        testPsfchMath();
        testCbrLevelsAndCr();
        testGrantOccasionTrains();
        testEnbScheduler();
        testUeRadioState();
        testPathPolicy();
        testStep6CandidateRepetitions();

        std::cout << "SlAlgorithmTest: ALL " << numChecks_ << " CHECKS PASSED" << std::endl;
    }

    void testEnbScheduler()
    {
        SlEnbScheduler::Config cfg;  // 5 subchannels x 10 PRB, mcs 6, oh 5, proc 3, 3 occasions gap 4
        int tbs1 = (int)SlMcsTable::tbsBytes(6, 10, 5);   // 123 B (verified in testMcsTable)
        int tbs2 = (int)SlMcsTable::tbsBytes(6, 20, 5);
        int tbsFull = (int)SlMcsTable::tbsBytes(6, 50, 5);

        // TB geometry: smallest width whose TBS covers min(reported, full-width TB)
        SlEnbScheduler s1(cfg);
        SlEnbScheduler::GrantSpec g1 = s1.onSlBsr(MacNodeId(1025), 100, 1000);
        check(g1.isValid() && g1.numSubchannels == 1 && g1.tbBytes == tbs1,
              "enbSched: 100 B fits one subchannel");
        SlEnbScheduler::GrantSpec g2 = s1.onSlBsr(MacNodeId(1026), tbs1 + 1, 1000);
        check(g2.isValid() && g2.numSubchannels == 2 && g2.tbBytes == tbs2,
              "enbSched: tbs1+1 B needs two subchannels");
        SlEnbScheduler::GrantSpec g3 = s1.onSlBsr(MacNodeId(1027), 100000, 1000);
        check(g3.isValid() && g3.numSubchannels == 5 && g3.tbBytes == tbsFull,
              "enbSched: huge backlog capped at one full-width TB");
        check(s1.onSlBsr(MacNodeId(1028), 0, 1000).isValid() == false, "enbSched: zero backlog -> no grant");

        // timing: first occasion >= now + ueProcessingSlots; spacing = occasionGapSlots
        check(g1.firstSlot >= 1000 + cfg.ueProcessingSlots, "enbSched: UE processing time respected");
        check(g1.numOccasions == 3 && g1.occasionGapSlots == 4, "enbSched: dynamic train shape from config");

        // no double-booking (dynamic x dynamic): same-slot trains must not overlap
        SlEnbScheduler s2(cfg);
        SlEnbScheduler::GrantSpec a = s2.onSlBsr(MacNodeId(1025), 3 * tbs1, 2000);  // width 3
        SlEnbScheduler::GrantSpec b = s2.onSlBsr(MacNodeId(1026), 3 * tbs1, 2000);  // width 3 does not fit beside a
        check(a.isValid() && b.isValid(), "enbSched: both requests served");
        bool overlap = (a.firstSlot == b.firstSlot) &&
                       (a.firstSubchannel < b.firstSubchannel + b.numSubchannels) &&
                       (b.firstSubchannel < a.firstSubchannel + a.numSubchannels);
        check(!overlap, "enbSched: no double-booking of the same slot");
        check(b.firstSlot != a.firstSlot || b.firstSubchannel != a.firstSubchannel,
              "enbSched: second train allocated elsewhere");
        SlEnbScheduler::GrantSpec c = s2.onSlBsr(MacNodeId(1027), 100, 2000);  // width 1 fits beside a width-3 train
        check(c.isValid() && (c.firstSlot == a.firstSlot ? c.firstSubchannel >= a.numSubchannels : true),
              "enbSched: narrow train packs into remaining subchannels");

        // grid pruning: commitments strictly before nowSlot are dropped lazily
        size_t before = s2.committedSlotCount();
        check(before > 0, "enbSched: grid holds commitments");
        s2.onSlBsr(MacNodeId(1028), 100, 100000);
        check(s2.committedSlotCount() < before + cfg.numOccasions,
              "enbSched: old commitments pruned on the next allocation");

        // horizon exhaustion: a saturated single-subchannel pool eventually refuses
        SlEnbScheduler::Config tiny = cfg;
        tiny.numSubchannels = 1;
        tiny.occasionGapSlots = 1;
        tiny.schedulingHorizonSlots = 8;
        SlEnbScheduler s3(tiny);
        int served = 0;
        for (int i = 0; i < 8; i++)
            if (s3.onSlBsr(MacNodeId(1025 + i), 50, 5000).isValid())
                served++;
        check(served > 0 && served < 8, "enbSched: saturated pool refuses past the horizon");

        testConfiguredGrants();
    }

    void testConfiguredGrants()
    {
        SlEnbScheduler::Config cfg;  // 5 subchannels x 10 PRB, mcs 6, proc 3
        int tbs1 = (int)SlMcsTable::tbsBytes(6, 10, 5);

        // CG shape: standing periodic train, first occasion within one period
        SlEnbScheduler s(cfg);
        auto cg1 = s.reserveConfiguredGrant(MacNodeId(1025), 40, 100, 0);
        check(cg1.isValid() && cg1.periodSlots == 40 && cg1.numOccasions == 0,
              "cg: standing train reserved (period 40)");
        check(cg1.firstSlot >= cfg.ueProcessingSlots && cg1.firstSlot < cfg.ueProcessingSlots + 40,
              "cg: phase within one period after processing time");
        check(cg1.numSubchannels == 1 && cg1.tbBytes == tbs1, "cg: TB geometry from tbBytes");

        // CG x CG (gcd test): same width, same phase would collide; the
        // allocator must pick a different phase or subchannel
        auto cg2 = s.reserveConfiguredGrant(MacNodeId(1026), 40, 100, 0);
        check(cg2.isValid(), "cg: second train reserved");
        bool disjoint = cg2.firstSubchannel != cg1.firstSubchannel ||
                        (cg2.firstSlot - cg1.firstSlot) % std::gcd((int64_t)40, (int64_t)40) != 0;
        check(disjoint, "cg: no CG x CG double-booking (gcd intersection)");

        // incommensurate periods: gcd(40, 60) = 20 - phases equal mod 20
        // collide even though the periods differ
        auto cg3 = s.reserveConfiguredGrant(MacNodeId(1027), 60, 100, 0);
        check(cg3.isValid(), "cg: mixed-period train reserved");
        bool clash13 = (cg3.firstSubchannel == cg1.firstSubchannel) &&
                       ((cg3.firstSlot >= cg1.firstSlot ? cg3.firstSlot - cg1.firstSlot : cg1.firstSlot - cg3.firstSlot)
                        % std::gcd((int64_t)60, (int64_t)40) == 0);
        bool clash23 = (cg3.firstSubchannel == cg2.firstSubchannel) &&
                       ((cg3.firstSlot >= cg2.firstSlot ? cg3.firstSlot - cg2.firstSlot : cg2.firstSlot - cg3.firstSlot)
                        % std::gcd((int64_t)60, (int64_t)40) == 0);
        check(!clash13 && !clash23, "cg: mixed-period trains avoid gcd-collisions");
        check(s.cgCount() == 3, "cg: three standing reservations");

        // dynamic x CG: dynamic allocations never land on a CG occasion
        for (int i = 0; i < 20; i++) {
            auto d = s.onSlBsr(MacNodeId(1030 + i), 5 * tbs1, (SlotIndex)i * 3);  // full width requests
            if (!d.isValid())
                continue;
            for (int k = 0; k < d.numOccasions; k++) {
                SlotIndex slot = d.firstSlot + (SlotIndex)k * d.occasionGapSlots;
                for (auto cg : { cg1, cg2, cg3 }) {
                    bool cgSlot = slot >= cg.firstSlot && (slot - cg.firstSlot) % cg.periodSlots == 0;
                    bool overlap = d.firstSubchannel < cg.firstSubchannel + cg.numSubchannels &&
                                   cg.firstSubchannel < d.firstSubchannel + d.numSubchannels;
                    if (cgSlot && overlap)
                        check(false, "cg: dynamic allocation landed on a CG occasion");
                }
            }
        }
        check(true, "cg: 20 full-width dynamic grants avoided all CG trains");

        // CG x dynamic: a new CG avoids existing dynamic commitments
        SlEnbScheduler s2(cfg);
        auto dyn = s2.onSlBsr(MacNodeId(1025), 5 * tbs1, 100);  // full width at slots ~103..111
        check(dyn.isValid() && dyn.numSubchannels == 5, "cg: full-width dynamic train placed");
        auto cgAfter = s2.reserveConfiguredGrant(MacNodeId(1026), 8, 100, 100);
        check(cgAfter.isValid(), "cg: train reserved beside dynamic commitments");
        bool hitsDyn = false;
        for (int k = 0; k < dyn.numOccasions; k++) {
            SlotIndex slot = dyn.firstSlot + (SlotIndex)k * dyn.occasionGapSlots;
            if (slot >= cgAfter.firstSlot && (slot - cgAfter.firstSlot) % cgAfter.periodSlots == 0)
                hitsDyn = true;  // same slot: needs disjoint subchannels
        }
        check(!hitsDyn || cgAfter.firstSubchannel >= dyn.firstSubchannel + dyn.numSubchannels ||
              dyn.firstSubchannel >= cgAfter.firstSubchannel + cgAfter.numSubchannels,
              "cg: standing train avoids dynamic commitments");

        // saturation: a 1-subchannel pool fits only one every-slot train
        SlEnbScheduler::Config tiny = cfg;
        tiny.numSubchannels = 1;
        SlEnbScheduler s4(tiny);
        check(s4.reserveConfiguredGrant(MacNodeId(1025), 1, 50, 0).isValid(), "cg: every-slot train fits an empty pool");
        check(!s4.reserveConfiguredGrant(MacNodeId(1026), 4, 50, 0).isValid(), "cg: full pool refuses further trains");
    }

    void testUeRadioState()
    {
        using RS = SlUeRadioState;
        RS rs;

        // no records: nothing overlaps
        check(!rs.overlapsTx(RS::UU, SimTime(0), SimTime(1, SIMTIME_MS)), "radioState: empty -> no overlap");

        // a Uu TX blocks overlapping SL-queried intervals, per-leg filtered
        rs.recordTx(RS::UU, SimTime(10, SIMTIME_MS), SimTime(11, SIMTIME_MS));
        check(rs.overlapsTx(RS::UU, SimTime(10500, SIMTIME_US), SimTime(11, SIMTIME_MS)), "radioState: overlap detected");
        check(!rs.overlapsTx(RS::SL, SimTime(10500, SIMTIME_US), SimTime(11, SIMTIME_MS)), "radioState: leg filter");

        // touching endpoints are NOT overlaps (back-to-back slots are legal)
        check(!rs.overlapsTx(RS::UU, SimTime(11, SIMTIME_MS), SimTime(115, SIMTIME_MS) / 10), "radioState: adjacent slot not blocked");
        check(!rs.overlapsTx(RS::UU, SimTime(9, SIMTIME_MS), SimTime(10, SIMTIME_MS)), "radioState: preceding slot not blocked");

        // TX-TX conflicts: recording an SL TX overlapping the Uu one counts
        bool conflict = rs.recordTx(RS::SL, SimTime(10200, SIMTIME_US), SimTime(10700, SIMTIME_US));
        check(conflict && rs.getTxConflicts() == 1, "radioState: TX-TX conflict counted");
        bool noConflict = rs.recordTx(RS::SL, SimTime(12, SIMTIME_MS), SimTime(125, SIMTIME_MS) / 10);
        check(!noConflict && rs.getTxConflicts() == 1, "radioState: disjoint TX is no conflict");

        // lazy pruning: records far older than the horizon are dropped on insert
        size_t before = rs.recordedCount();
        rs.recordTx(RS::UU, SimTime(1000, SIMTIME_MS), SimTime(1001, SIMTIME_MS));
        check(rs.recordedCount() < before + 1, "radioState: old records pruned on insert");
        check(rs.overlapsTx(RS::UU, SimTime(10005, SIMTIME_US) * 100, SimTime(10015, SIMTIME_US) * 100),
              "radioState: fresh record queryable");
    }

    void testPathPolicy()
    {
        using P = SlPathPolicy;
        P::Policy p;
        check(P::parse("pc5IfPeer", p) && p == P::PC5_IF_PEER, "pathPolicy: parse pc5IfPeer");
        check(P::parse("uuIfServed", p) && p == P::UU_IF_SERVED, "pathPolicy: parse uuIfServed");
        check(P::parse("pc5Only", p) && p == P::PC5_ONLY, "pathPolicy: parse pc5Only");
        check(P::parse("condition", p) && p == P::CONDITION, "pathPolicy: parse condition");
        check(!P::parse("bogus", p), "pathPolicy: unknown name rejected");

        // pc5IfPeer (the D16 default): capability decides, attachment ignored
        for (bool served : { false, true }) {
            check(P::decideUnicast(P::PC5_IF_PEER, true, served, false) == P::PATH_PC5,
                  "pathPolicy: pc5IfPeer routes SL peers over PC5");
            check(P::decideUnicast(P::PC5_IF_PEER, false, served, false) == P::PATH_UU,
                  "pathPolicy: pc5IfPeer routes non-peers over Uu");
        }

        // uuIfServed: attachment wins for capable peers
        check(P::decideUnicast(P::UU_IF_SERVED, true, true, false) == P::PATH_UU,
              "pathPolicy: uuIfServed prefers Uu while attached");
        check(P::decideUnicast(P::UU_IF_SERVED, true, false, false) == P::PATH_PC5,
              "pathPolicy: uuIfServed falls back to PC5 when detached");
        check(P::decideUnicast(P::UU_IF_SERVED, false, false, false) == P::PATH_UU,
              "pathPolicy: uuIfServed routes non-peers over Uu even detached");

        // pc5Only: no Uu fallback exists
        for (bool served : { false, true }) {
            check(P::decideUnicast(P::PC5_ONLY, true, served, false) == P::PATH_PC5,
                  "pathPolicy: pc5Only routes SL peers over PC5");
            check(P::decideUnicast(P::PC5_ONLY, false, served, false) == P::PATH_DENY,
                  "pathPolicy: pc5Only denies non-peer traffic");
        }

        // condition: the expression decides for capable peers only
        check(P::decideUnicast(P::CONDITION, true, true, true) == P::PATH_PC5,
              "pathPolicy: condition true -> PC5");
        check(P::decideUnicast(P::CONDITION, true, true, false) == P::PATH_UU,
              "pathPolicy: condition false -> Uu");
        check(P::decideUnicast(P::CONDITION, false, false, true) == P::PATH_UU,
              "pathPolicy: condition never routes non-peers over PC5");
    }

    void testStep6CandidateRepetitions()
    {
        // TS 38.214 §8.1.4 step 6: a reservation whose period exceeds the
        // selection window span must still exclude candidates whose OWN
        // future repetitions collide with it. Pool: 1 subchannel, window
        // [now+3, now+21] (19 candidates; T1 pinned at T_proc,1(mu=0) so the
        // Table 8.1.4-2 clamp is a no-op); own period 40 slots; sensed
        // reservation: period 200 slots (100 ms, the mode-1 CG train shape)
        // last heard at slot 850.
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 1;
        pool.t1 = 3;
        pool.t2 = 21;
        pool.numerology = 0;
        pool.rsrpThresholdDbm = -128;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(2000);
        db.recordSci(SlSensingEntry{850, 0, 1, -90, 200, MacNodeId(2049), 0});

        // from now=1000 the reservation's occurrences (1050, 1250, ...) are
        // all beyond the window - the pre-fix selector excluded nothing.
        // With step 6, candidate slot 1010 (= 1050 - 1*40 = 1250 - 6*40 ...)
        // is excluded: its own 40-slot train would hit every occurrence.
        auto sel = selector.select(1000, 1, 40, 20, db);
        check(sel.numCandidates == 19, "step6: M_total = 19");
        check(sel.numSurvivors == 18, "step6: the CG-aligned candidate is excluded");
        // the excluded slot is 1010: pick 7 (0-based over survivors
        // 1003..1009,1011...) lands on 1011, skipping 1010
        rng.fixed = 7;
        sel = selector.select(1000, 1, 40, 20, db);
        check(sel.slot == 1011, "step6: picks skip the protected CG phase");

        // a same-period reservation (both 40 slots) is caught identically
        // with and without step 6 (the shifts land on the same congruence
        // class): entry at 970 -> occurrence 1010 in-window, 1 exclusion
        SlSensingDatabase db2(2000);
        db2.recordSci(SlSensingEntry{970, 0, 1, -90, 40, MacNodeId(2050), 0});
        rng.fixed = 0;
        sel = selector.select(1000, 1, 40, 20, db2);
        check(sel.numSurvivors == 18, "step6: homogeneous periods unchanged (one congruence class)");

        // an own one-shot selection (ownPeriod 0 disables the j-loop): only
        // the in-window occurrence excludes
        SlSensingDatabase db3(2000);
        db3.recordSci(SlSensingEntry{850, 0, 1, -90, 200, MacNodeId(2049), 0});
        sel = selector.select(1000, 1, 0, 20, db3);
        check(sel.numSurvivors == 19, "step6: no own train -> no repetition shifts");
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
        // pool: 5 subchannels, window [now+3, now+12]; L_subCH=2 -> 4 positions
        // per slot, M_total = 40. T1 is pinned at T_proc,1(mu=0) = 3 so the
        // TS 38.214 Table 8.1.4-2 clamp is a no-op here (see testProcessingDelays).
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 5;
        pool.t1 = 3;
        pool.t2 = 12;
        pool.numerology = 0;
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
        check(sel.slot == 103 && sel.firstSubchannel == 0, "selector: pick 0 lands on the first candidate");

        // pick 8 must skip the 3 excluded candidates of slot 105 (indices
        // 8..10 in the slot-major grid) and land on (105, position 3)
        rng.fixed = 8;
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
        pool.t1 = 3;
        pool.t2 = 12;
        pool.numerology = 0;
        pool.rsrpThresholdDbm = -128;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        for (SlotIndex s = 93; s <= 102; s++)
            db.recordSci(SlSensingEntry{s, 0, 1, -90, 10, MacNodeId(2049), 0});

        auto sel = selector.select(100, 1, 200, 100, db);
        check(sel.numCandidates == 10, "20% rule: M_total = 10");
        check(sel.finalThresholdDbm == -89, "20% rule: threshold raised 13x3dB to -89");
        check(sel.numSurvivors == 10, "20% rule: all candidates recovered above the final threshold");
    }

    void testUnmonitoredExclusion()
    {
        // unmonitored slot 100 with allowed period 5 projects to slots 105 and
        // 110 inside the window [103,112]: both are conservatively excluded in
        // full (5 positions each). 40 of 50 survive, above X*M_total, so the
        // step-5a reset does not kick in.
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 5;
        pool.t1 = 3;
        pool.t2 = 12;
        pool.numerology = 0;
        pool.rsrpThresholdDbm = -128;
        pool.allowedPeriodsSlots = { 5 };
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        db.recordUnmonitoredSlot(100);

        auto sel = selector.select(100, 1, 40, 20, db);
        check(sel.numCandidates == 50, "unmonitored: M_total = 50");
        check(sel.numSurvivors == 40, "unmonitored: two whole slots conservatively excluded");
        check(!sel.step5aApplied, "unmonitored: step 5a not triggered when enough candidates remain");
    }

    void testStep5aReset()
    {
        // allowed period 1 + an unmonitored slot right before the window makes
        // the step-5 exclusion cover every candidate. TS 38.214 8.1.4 step 5a
        // then resets S_A to all candidates rather than letting the selection
        // starve: the unmonitored-slot exclusion is dropped, not the selection.
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 2;
        pool.t1 = 3;
        pool.t2 = 7;
        pool.numerology = 0;
        pool.rsrpThresholdDbm = -128;
        pool.allowedPeriodsSlots = { 1 };
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        SlSensingDatabase db(1600);
        db.recordUnmonitoredSlot(101);

        auto sel = selector.select(100, 1, 40, 20, db);
        check(sel.step5aApplied, "step 5a: the unmonitored-slot exclusion was dropped");
        check(sel.numSurvivors == sel.numCandidates, "step 5a: every candidate is selectable again");
        check(sel.slot >= 103 && sel.slot <= 107, "step 5a: pick inside the window");
    }

    void testProcessingDelays()
    {
        // TS 38.214 Table 8.1.4-2: the selection window cannot start earlier
        // than T_proc,1(mu) slots after n, whatever the pool asks for.
        check(SlMode2Selector::tProc1Slots(0) == 3 && SlMode2Selector::tProc1Slots(1) == 5
              && SlMode2Selector::tProc1Slots(2) == 9 && SlMode2Selector::tProc1Slots(3) == 17,
              "T_proc,1: Table 8.1.4-2 values");
        check(SlMode2Selector::tProc0Slots(0) == 1 && SlMode2Selector::tProc0Slots(1) == 1
              && SlMode2Selector::tProc0Slots(2) == 2 && SlMode2Selector::tProc0Slots(3) == 4,
              "T_proc,0: Table 8.1.4-1 values");

        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 1;
        pool.t1 = 1;              // below T_proc,1(mu=1) = 5
        pool.t2 = 12;
        pool.numerology = 1;
        pool.rsrpThresholdDbm = -128;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);
        SlSensingDatabase db(1600);

        auto sel = selector.select(100, 1, 40, 20, db);
        // window is [100+5, 100+12] = 8 slots, not the 12 the pool asked for
        check(sel.numCandidates == 8, "T_proc,1: selection window clamped to n+T_proc,1");
        check(sel.slot >= 105, "T_proc,1: no candidate earlier than n+T_proc,1");
    }

    void testTxPercentage()
    {
        // X is configurable (sl-TxPercentageList). With X = 0.5, a sensed
        // reservation covering more than half the window forces the step-7
        // threshold climb that X = 0.2 would have tolerated.
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = 1;
        pool.t1 = 3;
        pool.t2 = 12;
        pool.numerology = 0;
        pool.rsrpThresholdDbm = -128;
        pool.txPercentage = 0.5;
        FixedRandom rng;
        SlMode2Selector selector(pool, &rng);

        // reservations covering 7 of the 10 candidate slots at -90 dBm
        SlSensingDatabase db(1600);
        for (SlotIndex s = 103; s <= 109; s++)
            db.recordSci(SlSensingEntry{s - 10, 0, 1, -90, 10, MacNodeId(2049), 0});

        auto sel = selector.select(100, 1, 200, 100, db);
        check(sel.numCandidates == 10, "txPercentage: M_total = 10");
        check(sel.finalThresholdDbm > -128, "txPercentage: X=0.5 forces the 3 dB climb");
        check(sel.numSurvivors * 2 >= sel.numCandidates, "txPercentage: at least X*M_total survive");
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
