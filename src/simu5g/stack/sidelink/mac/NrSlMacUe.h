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
 * Phase SL-1 / WP-C scope: a static preconfigured periodic grant (slot offset
 * + subchannels from NED parameters); the mode-2 sensing-based selector
 * replaces it in WP-E. No HARQ (WP-F), one codeword, one SL carrier.
 */
class NrSlMacUe : public LteMacBase
{
  protected:
    SlRrc *slRrc_ = nullptr;
    SlBinder *slBinder_ = nullptr;

    SlSlotGrid slotGrid_;
    GHz carrierFrequency_;

    // static grant (WP-C): replaced by SlMode2Selector in WP-E
    SlGrant grant_;
    bool grantActive_ = false;
    int staticGrantSlotOffset_ = 0;
    int tbSize_ = 0;              // transport block size per TX opportunity [B] (WP-C stub for MCS/TBS)

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

    /// activate the static grant and/or make sure a TX-slot event is scheduled
    void ensureTxScheduled();

    /// TX-slot event: request one RLC PDU per backlogged SL connection
    void handleTxSlot();

    /// remove up to `bytes` bytes of announced data from a virtual buffer
    static void drainVirtualBuffer(LteMacBuffer *buffer, int64_t bytes);

  public:
    ~NrSlMacUe() override;
};

} // namespace simu5g

#endif
