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
#include "simu5g/stack/sidelink/mac/SlMcsTable.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrSlMacUe);

NrSlMacUe::~NrSlMacUe()
{
    cancelAndDelete(txSlotEvent_);
    delete selector_;
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

        std::string mode = par("resourceAllocationMode").stdstringValue();
        if (mode == "static")
            allocationMode_ = STATIC;
        else if (mode == "random")
            allocationMode_ = RANDOM;
        else if (mode == "mode2")
            allocationMode_ = MODE2;
        else
            throw cRuntimeError("NrSlMacUe: unknown resourceAllocationMode '%s' (expected mode2/random/static)", mode.c_str());

        staticGrantSlotOffset_ = par("staticGrantSlotOffset");
        grantNumSubchannels_ = par("grantNumSubchannels");
        probResourceKeep_ = par("probResourceKeep");
        tbSize_ = par("tbSize");
        computeTbSize_ = (tbSize_ < 0);
        subchannelSize_ = cfg.subchannelSize;
        overheadSymbols_ = par("overheadSymbols");

        periodMs_ = cfg.reservationPeriodsMs.empty() ? 100 : cfg.reservationPeriodsMs.front();
        periodSlots_ = slotGrid_.slotsPerMs(periodMs_);
        if (periodSlots_ <= 0)
            throw cRuntimeError("NrSlMacUe: reservation period is shorter than one slot");
        blindRetx_ = cfg.blindRetx;

        // sensing database + selector (used by mode2; random uses the selector
        // with an always-empty database)
        sensingDb_ = SlSensingDatabase(slotGrid_.slotsPerMs(cfg.t0Ms));
        SlMode2Selector::PoolConfig pool;
        pool.numSubchannels = cfg.numSubchannels;
        pool.t1 = cfg.t1;
        pool.t2 = cfg.t2;
        pool.rsrpThresholdDbm = cfg.rsrpThresholdDbm;
        for (int p : cfg.reservationPeriodsMs)
            pool.allowedPeriodsSlots.push_back(slotGrid_.slotsPerMs(p));
        selector_ = new SlMode2Selector(pool, &random_);

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

void NrSlMacUe::selectGrant()
{
    grant_.periodSlots = periodSlots_;
    grant_.mcs = par("grantMcs");
    grant_.blindRetx = 0;

    if (computeTbSize_) {
        // real link adaptation (D15): TBS follows the grant MCS and width
        int numPrbs = grantNumSubchannels_ * subchannelSize_;
        tbSize_ = SlMcsTable::tbsBytes(grant_.mcs, numPrbs, overheadSymbols_);
        EV << NOW << " NrSlMacUe::selectGrant - TBS " << tbSize_ << "B for MCS " << grant_.mcs
           << " over " << numPrbs << " PRBs (" << overheadSymbols_ << " overhead symbols)" << endl;
    }

    if (allocationMode_ == STATIC) {
        // WP-C static grant: fixed periodic resources from NED parameters
        grant_.firstSlot = staticGrantSlotOffset_;
        grant_.firstSubchannel = par("staticGrantFirstSubchannel");
        grant_.numSubchannels = grantNumSubchannels_;
        grant_.reselectionCounter = 0;  // never reselected
        EV << NOW << " NrSlMacUe::selectGrant - static grant: offset=" << grant_.firstSlot
           << " period=" << grant_.periodSlots << " slots, subchannels [" << grant_.firstSubchannel
           << ".." << grant_.firstSubchannel + grant_.numSubchannels - 1 << "]" << endl;
    }
    else {
        // mode 2 (TS 38.321 §5.22): sensing-based selection; the random
        // baseline uses the same selector with an empty sensing database
        SlotIndex now = slotGrid_.slotIndexAt(NOW);
        static const SlSensingDatabase emptyDb(0);
        const SlSensingDatabase& db = (allocationMode_ == MODE2) ? sensingDb_ : emptyDb;

        SlMode2Selector::Selection sel = selector_->select(now, grantNumSubchannels_, periodSlots_, periodMs_, db);
        grant_.firstSlot = sel.slot;
        grant_.firstSubchannel = sel.firstSubchannel;
        grant_.numSubchannels = grantNumSubchannels_;
        grant_.reselectionCounter = sel.reselectionCounter;

        EV << NOW << " NrSlMacUe::selectGrant - " << (allocationMode_ == MODE2 ? "mode-2" : "random")
           << " selection: slot " << grant_.firstSlot << ", subchannels [" << grant_.firstSubchannel
           << ".." << grant_.firstSubchannel + grant_.numSubchannels - 1 << "], period "
           << grant_.periodSlots << " slots, RC=" << grant_.reselectionCounter
           << " (candidates " << sel.numCandidates << ", survivors " << sel.numSurvivors
           << ", final threshold " << sel.finalThresholdDbm << " dBm)" << endl;
    }

    grantActive_ = true;
}

void NrSlMacUe::ensureTxScheduled()
{
    if (!grantActive_)
        selectGrant();

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
    SlotIndex slot = slotGrid_.slotIndexAt(NOW);
    EV << NOW << " NrSlMacUe::handleTxSlot - TX opportunity at slot " << slot << endl;

    ASSERT(requestedSdus_ == 0);

    // pending blind HARQ copies take the TX opportunity before new data
    // (WP-F; SL-1 simplification: retransmissions ride the same selected
    // resource train, i.e. the next period slots of the grant)
    SlHarqTxEntity::Retx retx;
    if (harqTx_.getNextRetx(retx)) {
        auto slInfo = retx.pdu->getTagForUpdate<SlTxInfoTag>();
        slInfo->setFirstSubchannel(grant_.firstSubchannel);   // current grant resources
        slInfo->setNumSubchannels(grant_.numSubchannels);
        slInfo->setHarqProcId(retx.procId);
        slInfo->setHarqNdi(retx.ndi);
        slInfo->setHarqRv(retx.rv);

        EV << NOW << " NrSlMacUe::handleTxSlot - blind retransmission of HARQ proc " << retx.procId
           << " (rv " << retx.rv << ")" << endl;
        sendLowerPackets(retx.pdu);

        if (allocationMode_ == MODE2)
            sensingDb_.recordUnmonitoredSlot(slot);
        if (allocationMode_ != STATIC && grant_.reselectionCounter > 0 && --grant_.reselectionCounter == 0) {
            if (random_.uniform01() < probResourceKeep_)
                grant_.reselectionCounter = selector_->drawReselectionCounter(periodMs_);
            else
                grantActive_ = false;
        }

        // new data (and further copies) wait for the next occasion
        for (auto& [cid, connInfo] : connDescOut_) {
            if (!connInfo.buffer->isEmpty()) {
                ensureTxScheduled();
                break;
            }
        }
        if (harqTx_.hasPendingRetx())
            ensureTxScheduled();
        return;
    }

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
    if (requestedSdus_ == 0)
        return;

    if (allocationMode_ == MODE2) {
        // half-duplex: this slot cannot be sensed (conservative exclusion input)
        sensingDb_.recordUnmonitoredSlot(slot);
    }

    if (allocationMode_ != STATIC && grant_.reselectionCounter > 0) {
        // SPS bookkeeping (TS 38.321 §5.22.1.1): the counter decrements per
        // transmission; on expiry the resource is kept with probResourceKeep
        // (fresh counter) or released for reselection
        if (--grant_.reselectionCounter == 0) {
            if (random_.uniform01() < probResourceKeep_) {
                grant_.reselectionCounter = selector_->drawReselectionCounter(periodMs_);
                EV << NOW << " NrSlMacUe::handleTxSlot - counter expired, resource KEPT (probResourceKeep), new RC="
                   << grant_.reselectionCounter << endl;
            }
            else {
                EV << NOW << " NrSlMacUe::handleTxSlot - counter expired, resource RELEASED for reselection" << endl;
                grantActive_ = false;
                // the pending TX of this slot still uses the old resources;
                // macPduMake() re-arms via ensureTxScheduled() -> reselection
            }
        }
    }
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
        slInfo->setReservationPeriodMs(periodMs_);

        macPkt->setTimestamp(NOW);

        while (!connInfo.queue->isEmpty()) {
            auto pkt = check_and_cast<Packet *>(connInfo.queue->popFront());
            drop(pkt);
            pkt->removeTagIfPresent<PdcpTrackingTag>();
            auto macPdu = macPkt->removeAtFront<LteMacPdu>();
            macPdu->pushSdu(pkt, destCid.getLcid());
            macPkt->insertAtFront(macPdu);
        }

        // register the TB with the blind-retx HARQ entity (WP-F); the stored
        // copy's stale HARQ/grant tag fields are re-stamped at retx time
        bool ndi;
        int procId = harqTx_.startTb(macPkt, blindRetx_, ndi);
        slInfo = macPkt->getTagForUpdate<SlTxInfoTag>();
        slInfo->setHarqProcId(procId);
        slInfo->setHarqNdi(ndi);
        slInfo->setHarqRv(0);

        EV << NOW << " NrSlMacUe::macPduMake - sending SL MAC PDU (" << macPkt->getByteLength()
           << "B) to slot " << slot << ", dstPid " << destCid.getNodeId()
           << ", HARQ proc " << procId << " (+" << blindRetx_ << " blind copies)" << endl;

        sendLowerPackets(macPkt);
    }

    // keep transmitting on the grant train while backlog or blind copies remain
    if (harqTx_.hasPendingRetx())
        ensureTxScheduled();
    else
        for (auto& [destCid, connInfo] : connDescOut_) {
            if (!connInfo.buffer->isEmpty()) {
                ensureTxScheduled();
                break;
            }
        }
}

void NrSlMacUe::onSciDecoded(const SlSensingEntry& entry)
{
    Enter_Method_Silent("onSciDecoded()");
    if (allocationMode_ == MODE2)
        sensingDb_.recordSci(entry);
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
