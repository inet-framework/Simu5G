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

NrSlPhyUe::~NrSlPhyUe()
{
    cancelAndDelete(decodeTimer_);
    for (auto *frame : storedFrames_)
        delete frame;
}

void NrSlPhyUe::initialize(int stage)
{
    if (stage == INITSTAGE_SIMU5G_REGISTRATIONS2) {
        // Deliberately skip LtePhyBase's initializeChannelModel(): it would
        // register the SL carrier in Binder's Uu carrier registry, silently
        // changing Uu MAC timing (gap G8). The SL carrier geometry lives in
        // SlBinder instead; the SL channel model plugs in here in WP-D.
        ChannelAccess::initialize(stage);

        slBinder_->registerSlPhy(nodeId_, this);
        return;
    }

    LtePhyBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        nodeId_ = MacNodeId(getContainingNode(this)->par("nrMacNodeId").intValue());
        nodeType_ = UE;
        txPower_ = ueTxPower_;

        slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
        slBinder_ = SlBinder::getInstance();

        const SlPreconfig& cfg = slRrc_->getPreconfig();
        slotGrid_ = SlSlotGrid(cfg.getSlotDuration());
        carrierFrequency_ = GHz(cfg.carrierFrequencyGHz);

        slTxRange_ = par("slTxRange").doubleValue();
        cbrWindow_ = par("cbrWindow");
        cbrRssiThresholdDbm_ = par("cbrRssiThreshold").doubleValue();

        slChannelModel_ = dynamic_cast<ISlChannelModel *>(getModuleByPath(par("slChannelModelModule").stringValue()));
        if (slChannelModel_ == nullptr)
            throw cRuntimeError("NrSlPhyUe: module '%s' does not implement ISlChannelModel", par("slChannelModelModule").stringValue());

        // sensing feed target (tolerates a custom SL MAC type: no feed then)
        slMac_ = dynamic_cast<NrSlMacUe *>(getModuleByPath(par("slMacModule").stringValue()));

        WATCH(numFramesHalfDuplexDropped_);
        WATCH(numSciLost_);
        WATCH(numDuplicatesSuppressed_);
    }
}

void NrSlPhyUe::handleUpperMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);
    auto lteInfo = pkt->removeTag<UserControlInfo>();
    auto slTxInfo = pkt->removeTag<SlTxInfoTag>();

    ASSERT(lteInfo->getFrameType() == DATAPKT && lteInfo->getDirection() == SL);

    SlotIndex slot = slotGrid_.slotIndexAt(NOW);
    lastTxSlot_ = slot;  // half-duplex: this node cannot receive in this slot

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
    info.setTxPower(txPower_);
    info.setSenderCoord(getRadioPosition());
    info.setCarrierFrequency(carrierFrequency_);

    // record the transmission for interference computation / sensing (D9);
    // prune records older than a generous sensing horizon (1600 slots > T0)
    SlBinder::SlTxRecord record{nodeId_, info.getFirstSubchannel(), info.getNumSubchannels(), txPower_, getRadioPosition()};
    slBinder_->recordSlTransmission(carrierFrequency_, slot, record, slot - 1600);

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
    auto *frame = check_and_cast<SlAirFrame *>(msg);

    EV << NOW << " NrSlPhyUe::handleAirFrame - stored SL frame from node "
       << frame->getInfo().getSrcNodeId() << " (slot " << frame->getInfo().getSlotIndex() << ")" << endl;

    storedFrames_.push_back(frame);

    if (decodeTimer_ == nullptr) {
        decodeTimer_ = new cMessage("slDecodeTimer");
        decodeTimer_->setSchedulingPriority(airFramePriority_);  // FIFO puts it after all arrivals of this slot
    }
    if (!decodeTimer_->isScheduled())
        scheduleAt(NOW, decodeTimer_);
}

void NrSlPhyUe::handleSelfMessage(cMessage *msg)
{
    ASSERT(msg == decodeTimer_);
    decodeStoredFrames();
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

        // blind-HARQ bookkeeping (WP-F): count the reception attempt for this
        // TB; drop copies of already-delivered TBs
        int attempt = harqRx_.onReception(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi());
        if (harqRx_.isDelivered(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi())) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - duplicate copy of an already-delivered TB"
               << " (src " << info.getSrcNodeId() << ", proc " << info.getHarqProcId() << "), suppressed" << endl;
            numDuplicatesSuppressed_++;
            delete frame;
            continue;
        }

        // PSSCH decode: BLER-curve TB error probability, soft-combined over
        // the attempts of this TB (the Uu harqReduction convention)
        double effErrorProb = rx.tbErrorProb * pow(slChannelModel_->getHarqReduction(), attempt - 1);
        bool tbDecoded = (effErrorProb <= 0) || (effErrorProb < 1 && uniform(0.0, 1.0) >= effErrorProb);
        emit(slFrameLossSignal_, tbDecoded ? 0.0 : 1.0);
        if (!tbDecoded) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - TB from node " << info.getSrcNodeId()
               << " lost (sinr " << rx.sinrDb << " dB, attempt " << attempt
               << ", eff. error prob " << effErrorProb << ")" << endl;
            numAirFrameNotReceived_++;
            delete frame;
            continue;
        }
        harqRx_.markDelivered(info.getSrcNodeId(), info.getHarqProcId(), info.getHarqNdi());

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

    if (slot != SLOTINDEX_NONE && slot != lastTxSlot_)
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
}

} // namespace simu5g
