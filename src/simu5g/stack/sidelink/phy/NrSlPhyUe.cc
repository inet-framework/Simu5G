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
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrSlPhyUe);

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

        WATCH(numFramesHalfDuplexDropped_);
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
    for (auto *frame : storedFrames_) {
        const SlAirFrameInfo& info = frame->getInfo();

        // half-duplex (D10): frames of a slot we transmitted in are lost
        if (info.getSlotIndex() == lastTxSlot_) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - half-duplex: own TX in slot "
               << info.getSlotIndex() << ", frame from node " << info.getSrcNodeId() << " lost" << endl;
            numFramesHalfDuplexDropped_++;
            delete frame;
            continue;
        }

        // WP-C ideal decode: SCI and PSSCH of every stored frame are received.
        // (WP-D replaces this with SL-RSRP measurement, the sensing database
        // update, PSCCH threshold decode and PSSCH BLER.)

        if (!isForUs(frame)) {
            EV << NOW << " NrSlPhyUe::decodeStoredFrames - frame for dstPid " << info.getDstPid()
               << " is not for us, dropped after SCI processing" << endl;
            delete frame;
            continue;
        }

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

    if (getEnvir()->isGUI())
        updateDisplayString();
}

} // namespace simu5g
