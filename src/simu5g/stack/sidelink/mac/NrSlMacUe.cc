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

#include "simu5g/stack/sidelink/mac/NrSlMacUe.h"

#include <inet/common/ModuleAccess.h>

#include "simu5g/stack/mac/buffer/LteMacBuffer.h"
#include "simu5g/stack/mac/buffer/LteMacQueue.h"
#include "simu5g/stack/phy/LtePhyBase.h"
#include "simu5g/stack/mac/packet/LteMacPdu.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/sidelink/common/SlAirFrame_m.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrSlMacUe);

NrSlMacUe::~NrSlMacUe()
{
    cancelAndDelete(txSlotEvent_);
}

void NrSlMacUe::initialize(int stage)
{
    LteMacBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        nodeId_ = MacNodeId(networkNode_->par("nrMacNodeId").intValue());
        nodeType_ = UE;
        isNr_ = true;
        cellId_ = NODEID_NONE;  // sidelink needs no serving cell

        slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
        slBinder_ = SlBinder::getInstance();

        const SlPreconfig& cfg = slRrc_->getPreconfig();
        slotGrid_ = SlSlotGrid(cfg.getSlotDuration());
        carrierFrequency_ = GHz(cfg.carrierFrequencyGHz);

        staticGrantSlotOffset_ = par("staticGrantSlotOffset");
        tbSize_ = par("tbSize");

        txSlotEvent_ = new cMessage("slTxSlot");
    }
}

void NrSlMacUe::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // the base class per-TTI dispatch is bypassed: this MAC is event-driven
        ASSERT(msg == txSlotEvent_);
        handleTxSlot();
        return;
    }
    LteMacBase::handleMessage(msg);
}

void NrSlMacUe::handleSelfMessage()
{
    throw cRuntimeError("NrSlMacUe::handleSelfMessage - unreachable: no ttiTick_ exists in the SL MAC");
}

void NrSlMacUe::ensureTxScheduled()
{
    if (!grantActive_) {
        // WP-C static grant: fixed periodic resources from the preconfiguration
        const SlPreconfig& cfg = slRrc_->getPreconfig();
        grant_.periodSlots = slotGrid_.slotsPerMs(cfg.reservationPeriodsMs.empty() ? 100 : cfg.reservationPeriodsMs.front());
        if (grant_.periodSlots <= 0)
            throw cRuntimeError("NrSlMacUe: reservation period is shorter than one slot");
        grant_.firstSlot = staticGrantSlotOffset_;
        grant_.firstSubchannel = par("staticGrantFirstSubchannel");
        grant_.numSubchannels = par("staticGrantNumSubchannels");
        grant_.mcs = par("staticGrantMcs");
        grant_.reselectionCounter = 0;  // static grant, never reselected
        grant_.blindRetx = 0;
        grantActive_ = true;
        EV << NOW << " NrSlMacUe::ensureTxScheduled - static grant activated: offset=" << grant_.firstSlot
           << " period=" << grant_.periodSlots << " slots, subchannels [" << grant_.firstSubchannel
           << ".." << grant_.firstSubchannel + grant_.numSubchannels - 1 << "]" << endl;
    }

    if (!txSlotEvent_->isScheduled()) {
        SlotIndex now = slotGrid_.slotIndexAt(NOW);
        SlotIndex next = slotGrid_.nextOccasionAfter(now, grant_.firstSlot, grant_.periodSlots);
        scheduleAt(slotGrid_.slotStart(next), txSlotEvent_);
        EV << NOW << " NrSlMacUe::ensureTxScheduled - next TX opportunity at slot " << next
           << " (t=" << slotGrid_.slotStart(next) << ")" << endl;
    }
}

void NrSlMacUe::handleTxSlot()
{
    EV << NOW << " NrSlMacUe::handleTxSlot - TX opportunity at slot " << slotGrid_.slotIndexAt(NOW) << endl;

    ASSERT(requestedSdus_ == 0);

    for (auto& [cid, connInfo] : connDescOut_) {
        if (connInfo.buffer->isEmpty())
            continue;

        // request one RLC PDU sized to the transport block (single-SDU-per-PDU
        // WP-C simplification; grant filling/LCP comes with the real scheduler)
        int64_t backlog = connInfo.buffer->getQueueOccupancy();
        unsigned int sduSize = (unsigned int)std::min((int64_t)tbSize_ - (int64_t)MAC_HEADER, backlog + RLC_HEADER_UM);

        EV << NOW << " NrSlMacUe::handleTxSlot - requesting SDU of " << sduSize << "B for connection " << cid << endl;

        auto pkt = new Packet("LteMacSduRequest");
        auto macSduRequest = makeShared<LteMacSduRequest>();
        macSduRequest->setChunkLength(b(1));
        macSduRequest->setUeId(cid.getNodeId());
        macSduRequest->setLcid(cid.getLcid());
        macSduRequest->setSduSize(sduSize);
        pkt->insertAtFront(macSduRequest);
        *(pkt->addTag<FlowControlInfo>()) = connInfo.flowInfo.toFlowControlInfo();
        sendUpperPackets(pkt);

        drainVirtualBuffer(connInfo.buffer, sduSize - RLC_HEADER_UM);
        requestedSdus_++;
    }

    // if nothing was requested, the event goes dormant; new data re-arms it
}

void NrSlMacUe::drainVirtualBuffer(LteMacBuffer *buffer, int64_t bytes)
{
    while (bytes > 0 && !buffer->isEmpty()) {
        PacketInfo vpkt = buffer->popFront();
        bytes -= vpkt.first;
    }
}

void NrSlMacUe::handleUpperMessage(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    bool isNewDataInd = (pkt->findTag<LteRlcNewDataTag>() != nullptr);

    bufferizePacket(pkt);

    if (isNewDataInd) {
        // arrival of new RLC data: make sure a TX opportunity is scheduled
        ensureTxScheduled();
    }
    else {
        requestedSdus_--;
        ASSERT(requestedSdus_ >= 0);
        // build MAC PDUs only after all requested SDUs have arrived from RLC
        if (requestedSdus_ == 0)
            macPduMake();
    }
}

bool NrSlMacUe::bufferizePacket(cPacket *cpkt)
{
    auto pkt = check_and_cast<Packet *>(cpkt);

    if (pkt->getBitLength() <= 1) { // empty "no data" reply from RLC
        delete pkt;
        return false;
    }

    pkt->setTimestamp();

    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    MacCid cid = MacCid(lteInfo->getDestId(), drbIdToLcid(lteInfo->getDrbId()));
    ASSERT(connDescOut_.find(cid) != connDescOut_.end());

    OutgoingConnectionInfo& connInfo = connDescOut_.at(cid);

    if (pkt->findTag<LteRlcNewDataTag>()) {
        // notification of new data in the RLC buffer: track it in the virtual buffer
        pkt->removeTag<LteRlcNewDataTag>();
        auto pdcpTag = pkt->getTag<PdcpTrackingTag>();
        PacketInfo vpkt(pdcpTag->getOriginalPacketLength(), pkt->getTimestamp());
        connInfo.buffer->pushBack(vpkt);
        delete pkt;
        return true;
    }

    // a real RLC PDU: queue it for the PDU maker
    if (!connInfo.queue->pushBack(pkt)) {
        totalOverflowedBytes_ += pkt->getByteLength();
        double sample = (double)totalOverflowedBytes_ / (NOW - getSimulation()->getWarmupPeriod());
        recordBufferOverflow((Direction)lteInfo->getDirection(), sample);
        EV << NOW << " NrSlMacUe::bufferizePacket - queue full for " << cid << ", dropping" << endl;
        delete pkt;
        return false;
    }
    return true;
}

void NrSlMacUe::macPduMake(MacCid cid)
{
    const SlPreconfig& cfg = slRrc_->getPreconfig();
    SlotIndex slot = slotGrid_.slotIndexAt(NOW);

    for (auto& [destCid, connInfo] : connDescOut_) {
        if (connInfo.queue->isEmpty())
            continue;

        auto macPkt = new Packet("SlMacPdu");
        auto header = makeShared<LteMacPdu>();
        header->setHeaderLength(MAC_HEADER);  // stand-in for the SL-SCH subheader (SRC/DST L2 ids)
        macPkt->insertAtFront(header);

        auto uinfo = macPkt->addTagIfAbsent<UserControlInfo>();
        uinfo->setSourceId(nodeId_);
        uinfo->setDestId(destCid.getNodeId());
        uinfo->setDirection(SL);
        uinfo->setFrameType(DATAPKT);
        uinfo->setCarrierFrequency(carrierFrequency_);

        const FlowDescriptor& flowInfo = connInfo.flowInfo;
        auto slInfo = macPkt->addTagIfAbsent<SlTxInfoTag>();
        slInfo->setSrcL2Id(flowInfo.getSlSrcL2Id());
        slInfo->setDstL2Id(flowInfo.getSlDstL2Id());
        slInfo->setCastType(flowInfo.getSlCastType());
        slInfo->setFirstSubchannel(grant_.firstSubchannel);
        slInfo->setNumSubchannels(grant_.numSubchannels);
        slInfo->setMcs(grant_.mcs);
        slInfo->setReservationPeriodMs(cfg.reservationPeriodsMs.empty() ? 0 : cfg.reservationPeriodsMs.front());

        macPkt->setTimestamp(NOW);

        while (!connInfo.queue->isEmpty()) {
            auto pkt = check_and_cast<Packet *>(connInfo.queue->popFront());
            drop(pkt);
            pkt->removeTagIfPresent<PdcpTrackingTag>();
            auto macPdu = macPkt->removeAtFront<LteMacPdu>();
            macPdu->pushSdu(pkt, destCid.getLcid());
            macPkt->insertAtFront(macPdu);
        }

        EV << NOW << " NrSlMacUe::macPduMake - sending SL MAC PDU (" << macPkt->getByteLength()
           << "B) to slot " << slot << ", dstPid " << destCid.getNodeId() << endl;

        sendLowerPackets(macPkt);
    }

    // keep transmitting on the grant train while backlog remains
    for (auto& [destCid, connInfo] : connDescOut_) {
        if (!connInfo.buffer->isEmpty()) {
            ensureTxScheduled();
            break;
        }
    }
}

void NrSlMacUe::fromPhy(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    auto userInfo = pkt->getTag<UserControlInfo>();

    // no HARQ in SL-1/WP-C: data goes straight to the PDU unmaker
    if (userInfo->getFrameType() != DATAPKT)
        throw cRuntimeError("NrSlMacUe::fromPhy - unexpected frame type %d", (int)userInfo->getFrameType());

    macPduUnmake(pkt);
}

void NrSlMacUe::macPduUnmake(cPacket *cpkt)
{
    auto pkt = check_and_cast<Packet *>(cpkt);
    take(pkt);
    auto macPdu = pkt->removeAtFront<LteMacPdu>();
    auto userInfo = pkt->getTag<UserControlInfo>();

    while (macPdu->hasSdu()) {
        LogicalCid lcid;
        auto upPkt = macPdu->popSdu(lcid);
        take(upPkt);

        MacNodeId senderId = userInfo->getSourceId();
        MacCid cid = MacCid(senderId, lcid);
        ASSERT(connDescIn_.find(cid) != connDescIn_.end());
        *upPkt->addTag<FlowControlInfo>() = connDescIn_[cid].toFlowControlInfo();

        sendUpperPackets(upPkt);
    }

    pkt->insertAtFront(macPdu);
    delete pkt;
}

} // namespace simu5g
