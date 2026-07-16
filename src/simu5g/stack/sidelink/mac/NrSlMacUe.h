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
#include "simu5g/stack/sidelink/mac/SlHarqTxEntity.h"
#include "simu5g/stack/sidelink/mac/SlMode2Selector.h"
#include "simu5g/stack/sidelink/mac/SlSensingDatabase.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

namespace simu5g {

class SlRrc;
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

    enum AllocationMode { STATIC, RANDOM, MODE2 };

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

    // blind-retransmission HARQ (WP-F)
    SlHarqTxEntity harqTx_;
    int blindRetx_ = 0;

    cMessage *txSlotEvent_ = nullptr;
    int requestedSdus_ = 0;

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

    /// remove up to `bytes` bytes of announced data from a virtual buffer
    static void drainVirtualBuffer(LteMacBuffer *buffer, int64_t bytes);

  public:
    ~NrSlMacUe() override;

    /// sensing input from slPhy: a decoded SCI with its measured SL-RSRP
    virtual void onSciDecoded(const SlSensingEntry& entry);
};

} // namespace simu5g

#endif
