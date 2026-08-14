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

#include "simu5g/stack/sidelink/phy/NrSlPhyUe.h"

#include <inet/common/ModuleAccess.h>

#include "simu5g/common/InitStages.h"
#include "simu5g/stack/sidelink/common/SlAirFrame_m.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/common/SlPsfch.h"
#include "simu5g/stack/sidelink/common/SlStatsCollector.h"
#include "simu5g/stack/sidelink/mac/NrSlMacUe.h"
#include "simu5g/stack/sidelink/phy/ISlChannelModel.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrSlPhyUe);

simsignal_t NrSlPhyUe::slRsrpSignal_ = registerSignal("slRsrp");
simsignal_t NrSlPhyUe::slSinrSignal_ = registerSignal("slSinr");
simsignal_t NrSlPhyUe::slCbrSignal_ = registerSignal("slCbr");
simsignal_t NrSlPhyUe::slFrameLossSignal_ = registerSignal("slFrameLoss");
simsignal_t NrSlPhyUe::slHalfDuplexUuDropsSignal_ = registerSignal("slHalfDuplexUuDrops");
simsignal_t NrSlPhyUe::slUuTxConflictsSignal_ = registerSignal("slUuTxConflicts");

NrSlPhyUe::~NrSlPhyUe()
{
    cancelAndDelete(decodeTimer_);
    cancelAndDelete(psfchTxTimer_);
    for (auto *frame : storedFrames_)
        delete frame;
    for (auto *frame : storedPsfchFrames_)
        delete frame;
}

void NrSlPhyUe::initialize(int stage)
{
    if (stage == INITSTAGE_SIMU5G_REGISTRATIONS2) {
        // Deliberately skip PhyBase's initializeChannelModel(): it would
        // register the SL carrier in Binder's Uu carrier registry, silently
        // changing Uu MAC timing (gap G8). The SL carrier geometry lives in
        // SlBinder instead; the SL channel model plugs in here in WP-D.
        ChannelAccess::initialize(stage);

        slBinder_->registerSlPhy(nodeId_, this);
        return;
    }

    if (stage == inet::INITSTAGE_LAST) {
        // G8 audit (SL-3 WP-M): the SL leg must never enter Binder's Uu
        // carrier registry - a registerCarrierUe() with the SL carrier would
        // silently change the Uu MAC's ttiPeriod_, which is derived from
        // binder->getUeMaxNumerologyIndex(nodeId). All SL carrier geometry
        // lives in SlBinder (registerSlCarrier) instead.
        bool inUuRegistry = false;
        try {
            const UeSet& ueSet = binder_->getCarrierUeSet(carrierFrequency_);
            inUuRegistry = ueSet.find(nodeId_) != ueSet.end();
        }
        catch (cRuntimeError&) {
            // SL carrier frequency not present in the Uu registry at all: OK
        }
        if (inUuRegistry)
            throw cRuntimeError("NrSlPhyUe: G8 violation - the SL carrier (%f GHz) is registered "
                                "in the Uu carrier registry for node %hu; this changes the Uu MAC's "
                                "slot timing. The SL pool must not be a Uu component carrier.",
                    carrierFrequency_.get(), num(nodeId_));
    }

    PhyBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        nodeId_ = MacNodeId(getContainingNode(this)->par("nrMacNodeId").intValue());
        nodeType_ = UE;
        txPower_ = ueTxPower_;

        slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
        slBinder_ = SlBinder::getInstance();

        slTxRange_ = par("slTxRange").doubleValue();
        cbrWindow_ = par("cbrWindow");
        cbrRssiThresholdDbm_ = par("cbrRssiThreshold").doubleValue();
        sharedUuSlRadio_ = par("sharedUuSlRadio");

        slChannelModel_ = dynamic_cast<ISlChannelModel *>(getModuleByPath(par("slChannelModelModule").stringValue()));
        if (slChannelModel_ == nullptr)
            throw cRuntimeError("NrSlPhyUe: module '%s' does not implement ISlChannelModel", par("slChannelModelModule").stringValue());

        // sensing feed target (tolerates a custom SL MAC type: no feed then)
        slMac_ = dynamic_cast<NrSlMacUe *>(getModuleByPath(par("slMacModule").stringValue()));

        // optional network-level PRR/PIR collector (WP-G)
        statsCollector_ = SlStatsCollector::findInstance();

        psfchTxTimer_ = new cMessage("slPsfchTx");

        WATCH(numFramesHalfDuplexDropped_);
        WATCH(numSciLost_);
        WATCH(numDuplicatesSuppressed_);
        WATCH(numPsfchSent_);
        WATCH(numPsfchLost_);
        WATCH(numPsfchDecoded_);
    }
    else if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS && sharedUuSlRadio_) {
        // D32: the shared radio state (created by SlRrc at INITSTAGE_LOCAL)
        radioState_ = slBinder_->getUeRadioState(nodeId_);
        if (radioState_ == nullptr)
            throw cRuntimeError("NrSlPhyUe: sharedUuSlRadio=true but node %hu has no radio state", num(nodeId_));
        WATCH(numSlHalfDuplexUuDrops_);
    }
    else if (stage == INITSTAGE_SIMU5G_TTI_SETUP) {
        // pool-geometry-dependent setup: runs strictly after SlRrc's pool
        // resolution at INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION (D25/G18)
        const SlPreconfig& cfg = slRrc_->getPreconfig();
        slotGrid_ = SlSlotGrid(cfg.getSlotDuration());
        carrierFrequency_ = GHz(cfg.carrierFrequencyGHz);
    }
}

void NrSlPhyUe::recordSlTx(SlotIndex slot)
{
    if (!sharedUuSlRadio_)
        return;
    bool conflict = radioState_->recordTx(SlUeRadioState::SL, slotGrid_.slotStart(slot), slotGrid_.slotStart(slot + 1));
    if (conflict) {
        emit(slUuTxConflictsSignal_, 1L);
        EV << NOW << " NrSlPhyUe::recordSlTx - SL TX overlaps a Uu TX (counted, not suppressed, D32)" << endl;
    }
}

bool NrSlPhyUe::uuTxOverlapsSlot(SlotIndex slot) const
{
    return sharedUuSlRadio_ &&
           radioState_->overlapsTx(SlUeRadioState::UU, slotGrid_.slotStart(slot), slotGrid_.slotStart(slot + 1));
}

void NrSlPhyUe::handleUpperMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);
    auto lteInfo = pkt->removeTag<UserControlInfo>();
    auto slTxInfo = pkt->removeTag<SlTxInfoTag>();

    ASSERT(lteInfo->getFrameType() == DATAPKT && lteInfo->getDirection() == SL);

    SlotIndex slot = slotGrid_.slotIndexAt(NOW);
    lastTxSlot_ = slot;  // half-duplex: this node cannot receive in this slot
    recordSlTx(slot);    // D32: visible to the Uu leg when the arbiter is on

    auto *frame = new SlAirFrame("slAirFrame");
    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(slotGrid_.getSlotDuration().dbl());
    frame->setCarrierFrequency(carrierFrequency_);
    frame->encapsulate(pkt);

    SlAirFrameInfo& info = frame->getInfoForUpdate();
    info.setSrcNodeId(nodeId_);
    info.setDstPid(lteInfo->getDestId());
    info.setSrcL2Id(slTxInfo->getSrcL2Id());
    info.setDstL2Id(slTxInfo->getDstL2Id());
    info.setCastType(slTxInfo->getCastType());
    info.setSlotIndex(slot);
    info.setFirstSubchannel(slTxInfo->getFirstSubchannel());
    info.setNumSubchannels(slTxInfo->getNumSubchannels());
    info.setMcs(slTxInfo->getMcs());
    info.setReservationPeriodMs(slTxInfo->getReservationPeriodMs());
    info.setHarqProcId(slTxInfo->getHarqProcId());
    info.setHarqNdi(slTxInfo->getHarqNdi());
    info.setHarqRv(slTxInfo->getHarqRv());
    double txPower = cappedTxPower();  // congestion control (D22) may cap it
    info.setTxPower(txPower);
    info.setSenderCoord(getRadioPosition());
    info.setCarrierFrequency(carrierFrequency_);

    // record the transmission for interference computation / sensing (D9);
    // prune records older than a generous sensing horizon (1600 slots > T0)
    SlBinder::SlTxRecord record{nodeId_, info.getFirstSubchannel(), info.getNumSubchannels(), txPower, getRadioPosition()};
    slBinder_->recordSlTransmission(carrierFrequency_, slot, record, slot - 1600);

    // PRR accounting (WP-G): register the receivers in range once per new TB
    if (statsCollector_ != nullptr && info.getHarqRv() == 0)
        statsCollector_->recordTransmission(nodeId_, getRadioPosition());

    // fan-out to every registered SL node in range: every SL PHY receives all
    // frames (the SCI must be decodable for sensing); the destination filter
    // is applied at slot-end decoding
    EV << NOW << " NrSlPhyUe::handleUpperMessage - transmitting SL frame in slot " << slot
       << ", subchannels [" << info.getFirstSubchannel() << "+" << info.getNumSubchannels() << "]" << endl;

    for (const auto& [destId, phyModule] : slBinder_->getSlPhys()) {
        if (destId == nodeId_)
            continue;

        if (slTxRange_ > 0) {
            auto *recvPhy = check_and_cast<NrSlPhyUe *>(phyModule);
            if (recvPhy->getRadioPosition().distance(getRadioPosition()) > slTxRange_)
                continue;
        }

        cModule *receiverNode = getContainingNode(phyModule);
        int gateId = receiverNode->findGate("slRadioIn");
        if (gateId < 0)
            throw cRuntimeError("NrSlPhyUe: receiver \"%s\" has no slRadioIn gate", receiverNode->getFullPath().c_str());

        sendDirect(frame->dup(), 0, frame->getDuration(), receiverNode, gateId);
    }

    lastActive_ = NOW;
    delete frame;
}

void NrSlPhyUe::handleAirFrame(cMessage *msg)
{
    if (auto *psfch = dynamic_cast<SlPsfchFrame *>(msg)) {
        EV << NOW << " NrSlPhyUe::handleAirFrame - stored PSFCH frame from node "
           << psfch->getFbSenderPid() << " (slot " << psfch->getSlotIndex()
           << ", resource " << psfch->getResourceIndex() << ")" << endl;
        storedPsfchFrames_.push_back(psfch);
    }
    else if (auto *frame = dynamic_cast<SlAirFrame *>(msg)) {
        EV << NOW << " NrSlPhyUe::handleAirFrame - stored SL frame from node "
           << frame->getInfo().getSrcNodeId() << " (slot " << frame->getInfo().getSlotIndex() << ")" << endl;
        storedFrames_.push_back(frame);
    }
    else {
        // a Uu channel-control broadcast (the gNB's handover beacon) leaking
        // onto the SL radio gate: the SL PHY is registered in the channel
        // control like every radio, but the SL leg is not a Uu receiver -
        // the beacon is measured by the Uu PHY legs, not here
        EV << NOW << " NrSlPhyUe::handleAirFrame - ignoring non-SL airframe '"
           << msg->getName() << "' (Uu broadcast)" << endl;
        delete msg;
        return;
    }

    if (decodeTimer_ == nullptr) {
        decodeTimer_ = new cMessage("slDecodeTimer");
        decodeTimer_->setSchedulingPriority(airFramePriority_);  // FIFO puts it after all arrivals of this slot
    }
    if (!decodeTimer_->isScheduled())
        scheduleAt(NOW, decodeTimer_);
}

void NrSlPhyUe::handleSelfMessage(cMessage *msg)
{
    if (msg == psfchTxTimer_) {
        transmitPendingPsfch();
        return;
    }
    ASSERT(msg == decodeTimer_);
    decodeStoredFrames();
    decodeStoredPsfchFrames();
}

bool NrSlPhyUe::isForUs(const SlAirFrame *frame) const
{
    const SlAirFrameInfo& info = frame->getInfo();
    if (info.getDstPid() == nodeId_)
        return true;  // unicast to us
    SlL2Id groupL2Id = slBinder_->getL2IdForGroupPid(info.getDstPid());
    return groupL2Id != SL_L2ID_NONE && slBinder_->isInGroup(groupL2Id, nodeId_);
}

void NrSlPhyUe::decodeStoredFrames()
{
    // all frames in the buffer belong to the slot that just ended
    SlotIndex slot = storedFrames_.empty() ? SLOTINDEX_NONE : storedFrames_.front()->getInfo().getSlotIndex();
    int numSubchannels = slRrc_->getPreconfig().numSubchannels;
    std::vector<double> rssiMw(numSubchannels, 0.0);

    // D32 half-duplex arbiter: a Uu transmission overlapping this SL slot
    // blots out the whole slot - frames are lost and the slot counts as
    // unmonitored for sensing, exactly like an own-TX slot
    bool uuBlocked = (slot != SLOTINDEX_NONE) && uuTxOverlapsSlot(slot);
    if (uuBlocked && slMac_ != nullptr)
        slMac_->onSlotUnmonitored(slot);

    for (auto *frame : storedFrames_) {
        const SlAirFrameInfo& info = frame->getInfo();

        // half-duplex (D10): frames of a slot we transmitted in are lost, and
        // the slot is not monitored (no measurements, no sensing input)
        if (info.getSlotIndex() == lastTxSlot_) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - half-duplex: own TX in slot "
               << info.getSlotIndex() << ", frame from node " << info.getSrcNodeId() << " lost" << endl;
            numFramesHalfDuplexDropped_++;
            delete frame;
            continue;
        }

        if (uuBlocked) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - half-duplex: the Uu leg transmitted "
               << "during slot " << info.getSlotIndex() << ", frame from node "
               << info.getSrcNodeId() << " lost (D32)" << endl;
            numSlHalfDuplexUuDrops_++;
            emit(slHalfDuplexUuDropsSignal_, 1L);
            delete frame;
            continue;
        }

        // measurements (WP-D): reception result from the SL channel model
        SlReceptionResult rx = slChannelModel_->computeReception(info, getRadioPosition(), nodeId_);
        emit(slRsrpSignal_, rx.rsrpDbm);
        emit(slSinrSignal_, rx.sinrDb);

        // accumulate per-subchannel RX power for the CBR (SL-RSSI) measurement
        double frameMw = pow(10.0, rx.rsrpDbm / 10.0);
        int last = std::min(info.getFirstSubchannel() + info.getNumSubchannels(), numSubchannels);
        for (int s = info.getFirstSubchannel(); s < last; s++)
            rssiMw[s] += frameMw;

        // PSCCH decode (threshold rule, D11): without the SCI nothing is known
        // about the frame (no sensing update, no PSSCH decode)
        if (!rx.sciDecoded) {
            numSciLost_++;
            numAirFrameNotReceived_++;
            delete frame;
            continue;
        }

        // sensing (WP-E): every decoded SCI feeds the MAC's sensing database
        if (slMac_ != nullptr) {
            SlSensingEntry entry;
            entry.slot = info.getSlotIndex();
            entry.firstSubchannel = info.getFirstSubchannel();
            entry.numSubchannels = info.getNumSubchannels();
            entry.rsrpDbm = rx.rsrpDbm;
            entry.reservationPeriodSlots = slotGrid_.slotsPerMs(info.getReservationPeriodMs());
            entry.srcNodeId = info.getSrcNodeId();
            slMac_->onSciDecoded(entry);
        }

        if (!isForUs(frame)) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - frame for dstPid " << info.getDstPid()
               << " is not for us, dropped after SCI processing" << endl;
            delete frame;
            continue;
        }

        // PSFCH (D19): does this transmission want HARQ feedback from us?
        SlPsfchMode fbMode = SL_PSFCH_OFF;
        double fbMcr = 0;
        int fbMemberIndex = 0;
        bool fbWanted = getFeedbackConfig(info, fbMode, fbMcr, fbMemberIndex);

        // blind-HARQ bookkeeping (WP-F): count the reception attempt for this
        // TB; drop copies of already-delivered TBs
        int attempt = harqRx_.onReception(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi());
        if (harqRx_.isDelivered(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi())) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - duplicate copy of an already-delivered TB"
               << " (src " << info.getSrcNodeId() << ", proc " << info.getHarqProcId() << "), suppressed" << endl;
            numDuplicatesSuppressed_++;
            // a retransmission of a TB we already have means our ACK was
            // lost: re-ACK it (NACK-only mode stays silent = success)
            if (fbWanted && fbMode == SL_PSFCH_ACK_NACK)
                schedulePsfchFeedback(info, true, fbMemberIndex);
            delete frame;
            continue;
        }

        // PSSCH decode: BLER-curve TB error probability, soft-combined over
        // the attempts of this TB (the Uu harqReduction convention)
        double effErrorProb = rx.tbErrorProb * pow(slChannelModel_->getHarqReduction(), attempt - 1);
        bool tbDecoded = (effErrorProb <= 0) || (effErrorProb < 1 && uniform(0.0, 1.0) >= effErrorProb);
        emit(slFrameLossSignal_, tbDecoded ? 0.0 : 1.0);

        if (fbWanted) {
            if (fbMode == SL_PSFCH_ACK_NACK) {
                // unicast / groupcast option 2: ACK or NACK per TB
                schedulePsfchFeedback(info, tbDecoded, fbMemberIndex);
            }
            else if (fbMode == SL_PSFCH_NACK_ONLY && !tbDecoded) {
                // groupcast option 1: NACK only, and only within the SLRB's
                // minimum communication range of the sender (TS 38.213-style
                // TX-RX distance gate on the SCI's sender position)
                double distance = getRadioPosition().distance(info.getSenderCoord());
                if (distance < fbMcr)
                    schedulePsfchFeedback(info, false, 0);
                else
                    EV << NOW << " NrSlPhyUe::decodeStoredFrames - TB lost but beyond MCR ("
                       << distance << " m >= " << fbMcr << " m), no NACK" << endl;
            }
        }

        if (!tbDecoded) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - TB from node " << info.getSrcNodeId()
               << " lost (sinr " << rx.sinrDb << " dB, attempt " << attempt
               << ", eff. error prob " << effErrorProb << ")" << endl;
            numAirFrameNotReceived_++;
            delete frame;
            continue;
        }
        harqRx_.markDelivered(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi());

        // PRR/PIR accounting (WP-G): one successful delivery per TB
        if (statsCollector_ != nullptr)
            statsCollector_->recordDelivery(info.getSrcNodeId(), nodeId_, info.getSenderCoord(), getRadioPosition());

        auto pkt = check_and_cast<Packet *>(frame->decapsulate());
        auto uinfo = pkt->addTagIfAbsent<UserControlInfo>();
        uinfo->setSourceId(info.getSrcNodeId());
        uinfo->setDestId(nodeId_);
        uinfo->setDirection(SL);
        uinfo->setFrameType(DATAPKT);
        uinfo->setCarrierFrequency(info.getCarrierFrequency());

        numAirFrameReceived_++;
        EV << NOW << " NrSlPhyUe::decodeStoredFrames - delivering SL MAC PDU from node "
           << info.getSrcNodeId() << " to the SL MAC" << endl;

        send(pkt, upperGateOut_);
        delete frame;
    }
    storedFrames_.clear();

    if (slot != SLOTINDEX_NONE && slot != lastTxSlot_ && !uuBlocked)
        recordSlotRssi(slot, std::move(rssiMw));

    if (getEnvir()->isGUI())
        updateDisplayString();
}

void NrSlPhyUe::recordSlotRssi(SlotIndex slot, std::vector<double>&& rssiMw)
{
    rssiHistory_.emplace_back(slot, std::move(rssiMw));

    // prune entries older than the CBR window
    while (!rssiHistory_.empty() && rssiHistory_.front().first <= slot - cbrWindow_)
        rssiHistory_.pop_front();

    // CBR (TS 38.215, abstracted): fraction of (subchannel, slot) resources in
    // the last cbrWindow_ slots whose RSSI exceeds the threshold; slots without
    // any reception count as idle
    double thresholdMw = pow(10.0, cbrRssiThresholdDbm_ / 10.0);
    int numSubchannels = slRrc_->getPreconfig().numSubchannels;
    int busy = 0;
    for (const auto& [recSlot, rssi] : rssiHistory_)
        for (double p : rssi)
            if (p > thresholdMw)
                busy++;

    double cbr = (double)busy / (cbrWindow_ * numSubchannels);
    emit(slCbrSignal_, cbr);

    // congestion control (D22): the MAC adapts its grants to the latest CBR
    lastCbr_ = cbr;
    if (slMac_ != nullptr)
        slMac_->onCbrUpdated(cbr);
}

double NrSlPhyUe::cappedTxPower() const
{
    const SlCbrLevel *level = slRrc_->getPreconfig().findCbrLevel(lastCbr_);
    return (level != nullptr) ? std::min(txPower_, level->maxTxPowerDbm) : txPower_;
}

bool NrSlPhyUe::getFeedbackConfig(const SlAirFrameInfo& info, SlPsfchMode& mode, double& mcrMeters, int& memberIndex) const
{
    const SlPreconfig& cfg = slRrc_->getPreconfig();
    if (cfg.psfchPeriod <= 0)
        return false;

    switch ((SlCastType)info.getCastType()) {
        case SL_UNICAST:
            // unicast always uses ACK/NACK when the pool has PSFCH (D19)
            mode = SL_PSFCH_ACK_NACK;
            mcrMeters = 0;
            memberIndex = 0;
            return true;

        case SL_GROUPCAST: {
            const SlrbConfigEntry *slrb = cfg.findSlrbForDstL2Id((SlL2Id)info.getDstL2Id());
            if (slrb == nullptr || slrb->psfchMode == SL_PSFCH_OFF)
                return false;
            mode = slrb->psfchMode;
            mcrMeters = slrb->mcrMeters;
            // option 2: this member's feedback resource offset = its rank in
            // the (ordered) group member set -- deterministic at TX and RX
            memberIndex = 0;
            for (MacNodeId member : slBinder_->getGroupMembers((SlL2Id)info.getDstL2Id())) {
                if (member == nodeId_)
                    break;
                memberIndex++;
            }
            return true;
        }

        default:
            return false;  // broadcast stays on blind retransmissions
    }
}

void NrSlPhyUe::schedulePsfchFeedback(const SlAirFrameInfo& info, bool ack, int memberIndex)
{
    const SlPreconfig& cfg = slRrc_->getPreconfig();
    SlotIndex fbSlot = slPsfchFeedbackSlot(info.getSlotIndex(), cfg.psfchPeriod, cfg.psfchMinGap);
    int resourceIndex = slPsfchResourceIndex(info.getSlotIndex(), info.getFirstSubchannel(),
            cfg.numSubchannels, memberIndex, cfg.psfchResources);

    EV << NOW << " NrSlPhyUe::schedulePsfchFeedback - " << (ack ? "ACK" : "NACK") << " for node "
       << info.getSrcNodeId() << " proc " << info.getHarqProcId() << " in PSFCH slot " << fbSlot
       << ", resource " << resourceIndex << endl;

    pendingPsfch_[fbSlot].push_back(PendingPsfch{info.getSrcNodeId(), info.getHarqProcId(), ack,
                                                 info.getCastType(), resourceIndex});

    simtime_t earliest = slotGrid_.slotStart(pendingPsfch_.begin()->first);
    ASSERT(earliest > NOW);  // decode runs at slot end; fbSlot >= psschSlot + psfchMinGap (>= 1)
    if (!psfchTxTimer_->isScheduled())
        scheduleAt(earliest, psfchTxTimer_);
    else if (psfchTxTimer_->getArrivalTime() > earliest) {
        cancelEvent(psfchTxTimer_);
        scheduleAt(earliest, psfchTxTimer_);
    }
}

void NrSlPhyUe::transmitPendingPsfch()
{
    SlotIndex slot = slotGrid_.slotIndexAt(NOW);
    auto it = pendingPsfch_.begin();
    ASSERT(it != pendingPsfch_.end() && it->first == slot);

    // half-duplex: a UE transmitting PSFCH cannot receive in this slot
    // (neither data nor other feedback -- the Rel-16 pain point), so it is
    // also unmonitored for mode-2 sensing, exactly like a data-TX slot
    lastTxSlot_ = slot;
    recordSlTx(slot);    // D32: visible to the Uu leg when the arbiter is on
    if (slMac_ != nullptr)
        slMac_->onSlotUnmonitored(slot);

    for (const PendingPsfch& fb : it->second) {
        auto phyIt = slBinder_->getSlPhys().find(fb.targetPid);
        if (phyIt == slBinder_->getSlPhys().end())
            continue;  // target gone

        // record on the reserved PSFCH band: co-resource feedbacks interfere,
        // the data band is never overlapped
        double txPower = cappedTxPower();  // congestion control (D22) may cap it
        SlBinder::SlTxRecord record{nodeId_, SL_PSFCH_BAND_BASE + fb.resourceIndex, 1, txPower, getRadioPosition()};
        slBinder_->recordSlTransmission(carrierFrequency_, slot, record, slot - 1600);

        auto *frame = new SlPsfchFrame("slPsfchFrame");
        frame->setSchedulingPriority(airFramePriority_);
        frame->setDuration(slotGrid_.getSlotDuration().dbl());
        frame->setCarrierFrequency(carrierFrequency_);
        frame->setFbSenderPid(nodeId_);
        frame->setTargetPid(fb.targetPid);
        frame->setHarqProcId(fb.harqProcId);
        frame->setAck(fb.ack);
        frame->setCastType(fb.castType);
        frame->setSlotIndex(slot);
        frame->setResourceIndex(fb.resourceIndex);
        frame->setTxPower(txPower);
        frame->setSenderCoord(getRadioPosition());

        EV << NOW << " NrSlPhyUe::transmitPendingPsfch - " << (fb.ack ? "ACK" : "NACK")
           << " to node " << fb.targetPid << " (proc " << fb.harqProcId
           << ", resource " << fb.resourceIndex << ")" << endl;

        if (slTxRange_ > 0) {
            auto *recvPhy = check_and_cast<NrSlPhyUe *>(phyIt->second);
            if (recvPhy->getRadioPosition().distance(getRadioPosition()) > slTxRange_) {
                delete frame;
                numPsfchSent_++;
                continue;
            }
        }

        // the feedback is addressed: direct delivery to the target only
        // (interference for other feedbacks comes from the tx map, not the frame)
        cModule *receiverNode = getContainingNode(phyIt->second);
        int gateId = receiverNode->findGate("slRadioIn");
        if (gateId < 0)
            throw cRuntimeError("NrSlPhyUe: receiver \"%s\" has no slRadioIn gate", receiverNode->getFullPath().c_str());
        sendDirect(frame, 0, frame->getDuration(), receiverNode, gateId);
        numPsfchSent_++;
    }
    pendingPsfch_.erase(it);
    lastActive_ = NOW;

    if (!pendingPsfch_.empty())
        scheduleAt(slotGrid_.slotStart(pendingPsfch_.begin()->first), psfchTxTimer_);
}

void NrSlPhyUe::decodeStoredPsfchFrames()
{
    for (auto *frame : storedPsfchFrames_) {
        ASSERT(frame->getTargetPid() == nodeId_);

        // half-duplex: own TX in the PSFCH slot loses the feedback (DTX at
        // the HARQ TX entity, handled by its deadline policy)
        if (frame->getSlotIndex() == lastTxSlot_) {
            EV << NOW << " NrSlPhyUe::decodeStoredPsfchFrames - half-duplex: own TX in slot "
               << frame->getSlotIndex() << ", PSFCH from node " << frame->getFbSenderPid() << " lost" << endl;
            numPsfchLost_++;
            delete frame;
            continue;
        }

        // D32: a Uu transmission overlapping the PSFCH slot loses the
        // feedback the same way (DTX at the HARQ TX entity)
        if (uuTxOverlapsSlot(frame->getSlotIndex())) {
            EV << NOW << " NrSlPhyUe::decodeStoredPsfchFrames - half-duplex: the Uu leg transmitted "
               << "during slot " << frame->getSlotIndex() << ", PSFCH from node "
               << frame->getFbSenderPid() << " lost (D32)" << endl;
            numPsfchLost_++;
            numSlHalfDuplexUuDrops_++;
            emit(slHalfDuplexUuDropsSignal_, 1L);
            delete frame;
            continue;
        }

        // threshold decode on the PSFCH band (interference = co-resource
        // feedback transmissions from the tx map)
        SlAirFrameInfo query;
        query.setSrcNodeId(frame->getFbSenderPid());
        query.setSlotIndex(frame->getSlotIndex());
        query.setFirstSubchannel(SL_PSFCH_BAND_BASE + frame->getResourceIndex());
        query.setNumSubchannels(1);
        query.setTxPower(frame->getTxPower());
        query.setSenderCoord(frame->getSenderCoord());
        query.setCarrierFrequency(frame->getCarrierFrequency());
        SlReceptionResult rx = slChannelModel_->computeReception(query, getRadioPosition(), nodeId_);

        if (rx.sinrDb < slChannelModel_->getPsfchSinrThresholdDb()) {
            EV << NOW << " NrSlPhyUe::decodeStoredPsfchFrames - PSFCH from node " << frame->getFbSenderPid()
               << " lost (sinr " << rx.sinrDb << " dB, resource " << frame->getResourceIndex() << ")" << endl;
            numPsfchLost_++;
            delete frame;
            continue;
        }

        numPsfchDecoded_++;
        EV << NOW << " NrSlPhyUe::decodeStoredPsfchFrames - " << (frame->getAck() ? "ACK" : "NACK")
           << " from node " << frame->getFbSenderPid() << " for proc " << frame->getHarqProcId() << endl;

        if (slMac_ != nullptr)
            slMac_->onPsfchDecoded(frame->getFbSenderPid(), frame->getHarqProcId(), frame->getAck());

        delete frame;
    }
    storedPsfchFrames_.clear();
}

} // namespace simu5g
