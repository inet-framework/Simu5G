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

#ifndef _SIDELINK_NRSLMACUE_H_
#define _SIDELINK_NRSLMACUE_H_

#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/mac/SlCrTracker.h"
#include "simu5g/stack/sidelink/mac/SlHarqTxEntity.h"
#include "simu5g/stack/sidelink/mac/SlMode2Selector.h"
#include "simu5g/stack/sidelink/mac/SlEnbScheduler.h"
#include "simu5g/stack/sidelink/mac/SlSensingDatabase.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

namespace simu5g {

class SlRrc;
class SlSchedulingGrant;
class SlBinder;

/**
 * Sidelink MAC of a UE (design decision D1): extends the abstract LteMacBase
 * root only (shared plumbing: gates, connection bookkeeping, RLC dispatch),
 * NOT the Uu UE MAC. It is fully event-driven: no per-slot ttiTick_ is ever
 * created; TX-slot self-events are scheduled at absolute slot times only
 * while there is backlogged data and an active grant.
 *
 * Resource allocation modes (resourceAllocationMode parameter):
 *  - "mode2":  TS 38.321 §5.22 sensing-based selection (SlMode2Selector fed
 *              by the SlSensingDatabase), SPS with reselection counter and
 *              probResourceKeep
 *  - "random": uniform random selection over the same window, no sensing --
 *              the community-standard baseline for sensing-gain comparisons
 *  - "static": fixed preconfigured periodic grant per UE (the WP-C pipe;
 *              slot offset + subchannels from NED parameters)
 *
 * No HARQ (WP-F), one codeword, one SL carrier, single-slot resources.
 */
class NrSlMacUe : public LteMacBase
{
  protected:
    /// ISlRandom adapter over the module RNG
    class ModuleRandom : public ISlRandom {
        cSimpleModule *module_;
      public:
        explicit ModuleRandom(cSimpleModule *module) : module_(module) {}
        double uniform01() override { return module_->uniform(0, 1); }
        int intuniform(int a, int b) override { return module_->intuniform(a, b); }
    };

    enum AllocationMode { STATIC, RANDOM, MODE2, MODE1 };

    SlRrc *slRrc_ = nullptr;
    SlBinder *slBinder_ = nullptr;

    SlSlotGrid slotGrid_;
    GHz carrierFrequency_;

    AllocationMode allocationMode_ = MODE2;
    ModuleRandom random_{this};
    SlSensingDatabase sensingDb_;
    SlMode2Selector *selector_ = nullptr;
    double probResourceKeep_ = 0;

    SlGrant grant_;
    bool grantActive_ = false;
    int staticGrantSlotOffset_ = 0;   // static mode only
    int grantNumSubchannels_ = 1;     // L_subCH of selected resources
    int periodSlots_ = 0;             // resource reservation period [slots]
    int periodMs_ = 0;
    int tbSize_ = 0;                  // transport block size per TX opportunity [B]
    bool computeTbSize_ = false;      // true = derive tbSize_ from the grant MCS via SlMcsTable (D15)
    int subchannelSize_ = 0;          // PRBs per subchannel (from the preconfig)
    int overheadSymbols_ = 0;         // non-data symbols per slot for the TBS math

    // HARQ TX entity: blind copies (WP-F) and PSFCH feedback mode (WP-I, D24)
    SlHarqTxEntity harqTx_;
    int blindRetx_ = 0;
    int psfchPeriod_ = 0;             // pool PSFCH configuration (0 = feedback off)
    int psfchMinGap_ = 0;
    int harqMaxRtx_ = 3;              // retransmission budget per feedback-mode TB
    bool dtxAsAck_ = false;           // missing feedback at the deadline: false = treat as NACK
    unsigned int numFeedbackRetx_ = 0;
    unsigned int numDtx_ = 0;

    // CBR-based congestion control (WP-K, D22; active when the preconfig
    // has a cbrConfig table)
    double lastCbr_ = 0;              // latest CBR pushed by slPhy
    SlCrTracker crTracker_;
    int crWindowSlots_ = 1000;
    unsigned int numCrDeferred_ = 0;  // TX occasions skipped over the CR limit

    cMessage *txSlotEvent_ = nullptr;
    /// closes a TX occasion once its SDU requests have been answered or declined
    cMessage *assembleTbEvent_ = nullptr;
    /// per logical channel: whether the RLC TX queue's head SDU is the remainder
    /// of one already segmented, which decides the next PDU's NR-SO header size
    std::map<MacCid, bool> soFrontIsContinuation_;
    int requestedSdus_ = 0;

    // mode 1 (D26/D30, SL-3): grant-cycle latency anchor - set by the Uu MAC
    // (NrMacUeSl) when the RAC of a new request cycle is sent, cleared and
    // measured when the grant arrives at onMode1Grant()
    simtime_t mode1CycleStart_ = -1;
    static omnetpp::simsignal_t slMode1GrantLatencySignal_;

    // configured grant (D30, WP-P): the standing train handed over by SlRrc
    // at pool-resolution time. Type 1 activates at this MAC's pool-init
    // stage; type 2 stays dormant until a cgAction=activate DCI and
    // self-releases after cgInactivityOccasions consecutive empty occasions
    // (models the DCI release without a reverse channel)
    SlGrant cgGrant_;
    bool cgConfigured_ = false;
    bool cgActive_ = false;
    int cgType_ = 0;
    int cgInactivityOccasions_ = 5;
    int cgIdleOccasions_ = 0;

    /// arm the stored CG train as the active grant
    void activateCg();

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;
    void handleSelfMessage() override;
    void handleUpperMessage(cPacket *pkt) override;
    bool bufferizePacket(cPacket *pkt) override;
    void fromPhy(cPacket *pkt) override;

    void macPduMake(MacCid cid = MacCid()) override;
    void macPduUnmake(cPacket *pkt) override;
    void updateUserTxParam(cPacket *pkt) override {}

    /// activate a grant (per the allocation mode) and/or make sure a TX-slot
    /// event is scheduled
    void ensureTxScheduled();

    /// (re)select the grant resources per the allocation mode
    void selectGrant();

    /// TX-slot event: request one RLC PDU per backlogged SL connection
    void handleTxSlot();

    /// the SCI resource-reservation value [ms] to stamp on a TX in `slot`
    /// (finite mode-1 trains advertise the occasion gap, 0 on the last, D30)
    int reservationPeriodMsAt(SlotIndex slot) const;

    /// finite-train bookkeeping (D30): on a finite grant's last occasion the
    /// grant is spent - called at the end of every consumed TX occasion
    void retireOccasionIfLast(SlotIndex slot);

    /// remove up to `bytes` bytes of announced data from a virtual buffer
    static void drainVirtualBuffer(LteMacBuffer *buffer, int64_t bytes);

  public:
    ~NrSlMacUe() override;

    /// sensing input from slPhy: a decoded SCI with its measured SL-RSRP
    virtual void onSciDecoded(const SlSensingEntry& entry);

    /// PSFCH input from slPhy: a decoded ACK/NACK for one of our HARQ
    /// processes (D24; acted upon once the feedback-driven TX entity lands)
    virtual void onPsfchDecoded(MacNodeId fbSender, int harqProcId, bool ack);

    /// CBR input from slPhy (D22): latest channel busy ratio measurement
    virtual void onCbrUpdated(double cbr);

    /// D32: slPhy reports a slot lost to a Uu transmission (half-duplex
    /// arbiter) - a conservative-exclusion sensing input like an own-TX slot
    virtual void onSlotUnmonitored(SlotIndex slot);

    // --- mode 1 (gNB-scheduled, D26/D30, SL-3): queried/driven by the Uu
    //     MAC subclass NrMacUeSl via direct C++ calls (the onSciDecoded
    //     pattern); the Uu side of the request loop lives there (G19) ---

    /// true iff this MAC runs mode 1, has unserved backlog and no
    /// active/pending grant - the single source of truth of the SL-BSR
    /// trigger chain (state-based: lost edges self-heal per checkRAC tick)
    virtual bool mode1BsrPending() const;

    /// aggregate SL backlog [B] for the SL-BSR: virtual-buffer occupancy
    /// plus RLC header per backlogged connection (the D2D summation, D26)
    virtual int mode1BsrBytes() const;

    /// latency-stat anchor: the Uu MAC reports that a request cycle started
    /// (RAC sent for the SL-BSR); no-op if a cycle is already running
    virtual void onMode1RequestStarted();

    /// a DCI 3_0-equivalent arrived over the Uu (routed here by
    /// NrMacUeSl::macHandleGrant, D28): populate the grant and arm the
    /// occasion train (or activate/release the stored CG per cgAction);
    /// the whole SL-1/SL-2 HARQ/PSFCH/LCP machinery hangs off the train
    /// unchanged (D30)
    virtual void onMode1Grant(const SlSchedulingGrant *slGrant);

    /// SlRrc hands over the cell-reserved configured grant (D30, WP-P);
    /// called at pool-resolution init time, before this MAC's pool init
    virtual void onConfiguredGrant(const SlEnbScheduler::GrantSpec& spec, int type);
};

} // namespace simu5g

#endif
