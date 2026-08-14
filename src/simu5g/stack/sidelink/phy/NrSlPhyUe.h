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
#include <map>

#include "simu5g/stack/phy/PhyBase.h"
#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/common/SlUeRadioState.h"
#include "simu5g/stack/sidelink/mac/SlHarqRxEntity.h"
#include "simu5g/stack/sidelink/mac/SlSlotGrid.h"

namespace simu5g {

class SlBinder;
class SlRrc;
class SlAirFrame;
class SlAirFrameInfo;
class SlPsfchFrame;
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
 * arrive in WP-D. The Uu channel-model initialization of PhyBase is
 * deliberately skipped so the SL carrier is never registered in Binder's Uu
 * carrier registry (gap G8).
 */
class NrSlPhyUe : public PhyBase
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
    double lastCbr_ = 0;   // latest measurement, also pushed to the MAC (D22)

    /// congestion control (D22): the current CBR level's TX power cap
    double cappedTxPower() const;

    // blind-retransmission HARQ RX bookkeeping (WP-F): attempt counting for
    // soft combining + duplicate-delivery suppression
    SlHarqRxEntity harqRx_;

    // PSFCH (D19): feedback pending transmission, keyed by its PSFCH slot;
    // psfchTxTimer_ fires at the start of the earliest pending slot
    struct PendingPsfch {
        MacNodeId targetPid = NODEID_NONE;
        int harqProcId = 0;
        bool ack = false;
        unsigned short castType = 0;
        int resourceIndex = 0;
    };
    std::map<SlotIndex, std::vector<PendingPsfch>> pendingPsfch_;
    cMessage *psfchTxTimer_ = nullptr;
    std::vector<SlPsfchFrame *> storedPsfchFrames_;

    // statistics
    unsigned int numFramesHalfDuplexDropped_ = 0;

    // Uu/SL half-duplex arbiter (D32, SL-3; opt-in via sharedUuSlRadio):
    // SL TX intervals are recorded in the UE's shared radio state, and SL
    // reception in a slot overlapped by a Uu transmission is lost (the
    // slot also counts as unmonitored for sensing, like an own-TX slot)
    bool sharedUuSlRadio_ = false;
    SlUeRadioState *radioState_ = nullptr;
    unsigned int numSlHalfDuplexUuDrops_ = 0;

    /// record this slot's SL transmission in the shared radio state
    void recordSlTx(SlotIndex slot);

    /// was any Uu transmission overlapping the given SL slot?
    bool uuTxOverlapsSlot(SlotIndex slot) const;
    unsigned int numSciLost_ = 0;
    unsigned int numDuplicatesSuppressed_ = 0;
    unsigned int numPsfchSent_ = 0;
    unsigned int numPsfchLost_ = 0;
    unsigned int numPsfchDecoded_ = 0;
    static simsignal_t slRsrpSignal_;
    static simsignal_t slSinrSignal_;
    static simsignal_t slCbrSignal_;
    static simsignal_t slFrameLossSignal_;
    static simsignal_t slHalfDuplexUuDropsSignal_;
    static simsignal_t slUuTxConflictsSignal_;

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

    // --- PSFCH (D19) ---

    /// whether a received transmission wants HARQ feedback from this node,
    /// and with which semantics (mode/mcr from the SLRB config for
    /// groupcast; unicast is implicitly ACK/NACK when the pool has PSFCH)
    bool getFeedbackConfig(const SlAirFrameInfo& info, SlPsfchMode& mode, double& mcrMeters, int& memberIndex) const;

    /// queue an ACK/NACK for transmission in the PSSCH's PSFCH slot
    void schedulePsfchFeedback(const SlAirFrameInfo& info, bool ack, int memberIndex);

    /// psfchTxTimer_ fired: transmit all feedback pending for this slot
    void transmitPendingPsfch();

    /// slot-end decode of received PSFCH frames (threshold rule on the
    /// PSFCH band, half-duplex applies); decoded feedback goes to the SL MAC
    void decodeStoredPsfchFrames();

  public:
    ~NrSlPhyUe() override;
};

} // namespace simu5g

#endif
