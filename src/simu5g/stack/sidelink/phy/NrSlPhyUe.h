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

#ifndef _SIDELINK_NRSLPHYUE_H_
#define _SIDELINK_NRSLPHYUE_H_

#include <deque>

#include "simu5g/stack/phy/LtePhyBase.h"
#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/mac/SlHarqRxEntity.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

namespace simu5g {

class SlBinder;
class SlRrc;
class SlAirFrame;
class ISlChannelModel;
class NrSlMacUe;
class SlStatsCollector;

/**
 * Sidelink PHY of a UE: PSCCH+PSSCH transmission as SlAirFrame fan-out to all
 * SL-capable nodes in range (sendDirect over the dedicated slRadioIn gate
 * chain), and store-and-decode-at-slot-end reception with half-duplex
 * bookkeeping (design decision D10).
 *
 * Phase SL-1 / WP-C scope: ideal decode (every frame in range is received,
 * no SINR/BLER); the TR 37.885 channel model and the SL interference map
 * arrive in WP-D. The Uu channel-model initialization of LtePhyBase is
 * deliberately skipped so the SL carrier is never registered in Binder's Uu
 * carrier registry (gap G8).
 */
class NrSlPhyUe : public LtePhyBase
{
  protected:
    SlBinder *slBinder_ = nullptr;
    SlRrc *slRrc_ = nullptr;
    ISlChannelModel *slChannelModel_ = nullptr;
    NrSlMacUe *slMac_ = nullptr;   // sensing feed; null when a custom SL MAC is swapped in
    SlStatsCollector *statsCollector_ = nullptr;   // PRR/PIR reporting; null when the network has none

    SlSlotGrid slotGrid_;
    GHz carrierFrequency_;

    double slTxRange_ = -1;  // fan-out pruning range [m]; <= 0 disables the check

    // store-and-decode-at-slot-end (D10)
    cMessage *decodeTimer_ = nullptr;
    std::vector<SlAirFrame *> storedFrames_;
    SlotIndex lastTxSlot_ = SLOTINDEX_NONE;  // half-duplex bookkeeping

    // CBR measurement (TS 38.215, WP-D): ring buffer of per-subchannel linear
    // RX power [mW] of the last cbrWindow_ monitored slots, filled at slot-end
    // processing; CBR is computed lazily from it (no per-slot ticker)
    std::deque<std::pair<SlotIndex, std::vector<double>>> rssiHistory_;
    int cbrWindow_ = 100;
    double cbrRssiThresholdDbm_ = -94;

    // blind-retransmission HARQ RX bookkeeping (WP-F): attempt counting for
    // soft combining + duplicate-delivery suppression
    SlHarqRxEntity harqRx_;

    // statistics
    unsigned int numFramesHalfDuplexDropped_ = 0;
    unsigned int numSciLost_ = 0;
    unsigned int numDuplicatesSuppressed_ = 0;
    static simsignal_t slRsrpSignal_;
    static simsignal_t slSinrSignal_;
    static simsignal_t slCbrSignal_;
    static simsignal_t slFrameLossSignal_;

    void initialize(int stage) override;
    void handleUpperMessage(cMessage *msg) override;
    void handleAirFrame(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg) override;

    /// slot-end processing: half-duplex check, measurements, decode, delivery filter
    void decodeStoredFrames();

    /// record this slot's per-subchannel RX power and emit the current CBR
    void recordSlotRssi(SlotIndex slot, std::vector<double>&& rssiMw);

    /// true if this node is a destination of the frame (own id or group member)
    bool isForUs(const SlAirFrame *frame) const;

  public:
    ~NrSlPhyUe() override;
};

} // namespace simu5g

#endif
