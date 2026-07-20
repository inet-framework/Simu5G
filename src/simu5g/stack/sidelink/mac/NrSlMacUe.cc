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

#include "simu5g/common/InitStages.h"

#include <algorithm>
#include <limits>

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
#include "simu5g/stack/sidelink/common/SlPsfch.h"
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
        overheadSymbols_ = par("overheadSymbols");

        harqMaxRtx_ = par("harqMaxRtx");
        dtxAsAck_ = (par("psfchDtxPolicy").stdstringValue() == "ack");
        WATCH(numFeedbackRetx_);
        WATCH(numDtx_);
        WATCH(numCrDeferred_);

        txSlotEvent_ = new cMessage("slTxSlot");
    }
    else if (stage == INITSTAGE_SIMU5G_TTI_SETUP) {
        // pool-geometry-dependent setup: runs strictly after SlRrc's pool
        // resolution at INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION (D25/G18) -
        // with poolSource="servingCell" the pool section only settles there
        const SlPreconfig& cfg = slRrc_->getPreconfig();
        slotGrid_ = SlSlotGrid(cfg.getSlotDuration());
        carrierFrequency_ = GHz(cfg.carrierFrequencyGHz);
        subchannelSize_ = cfg.subchannelSize;

        periodMs_ = cfg.reservationPeriodsMs.empty() ? 100 : cfg.reservationPeriodsMs.front();
        periodSlots_ = slotGrid_.slotsPerMs(periodMs_);
        if (periodSlots_ <= 0)
            throw cRuntimeError("NrSlMacUe: reservation period is shorter than one slot");
        blindRetx_ = cfg.blindRetx;
        psfchPeriod_ = cfg.psfchPeriod;
        psfchMinGap_ = cfg.psfchMinGap;

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
    grant_.numOccasions = 0;       // self-selected grants are unbounded trains
    grant_.occasionGapSlots = 0;   // (finite trains are mode-1-issued, D30)

    // congestion control (D22): the current CBR level caps MCS and L_subCH
    // at (re)selection
    int numSubchannels = grantNumSubchannels_;
    const SlCbrLevel *level = slRrc_->getPreconfig().findCbrLevel(lastCbr_);
    if (level != nullptr) {
        if (grant_.mcs > level->maxMcs || numSubchannels > level->maxNumSubchannels)
            EV << NOW << " NrSlMacUe::selectGrant - CBR " << lastCbr_ << " caps the grant to MCS "
               << level->maxMcs << ", " << level->maxNumSubchannels << " subchannel(s)" << endl;
        grant_.mcs = std::min(grant_.mcs, level->maxMcs);
        numSubchannels = std::min(numSubchannels, level->maxNumSubchannels);
    }

    if (computeTbSize_) {
        // real link adaptation (D15): TBS follows the grant MCS and width
        int numPrbs = numSubchannels * subchannelSize_;
        tbSize_ = SlMcsTable::tbsBytes(grant_.mcs, numPrbs, overheadSymbols_);
        EV << NOW << " NrSlMacUe::selectGrant - TBS " << tbSize_ << "B for MCS " << grant_.mcs
           << " over " << numPrbs << " PRBs (" << overheadSymbols_ << " overhead symbols)" << endl;
    }

    if (allocationMode_ == STATIC) {
        // WP-C static grant: fixed periodic resources from NED parameters
        grant_.firstSlot = staticGrantSlotOffset_;
        grant_.firstSubchannel = par("staticGrantFirstSubchannel");
        grant_.numSubchannels = numSubchannels;
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

        SlMode2Selector::Selection sel = selector_->select(now, numSubchannels, periodSlots_, periodMs_, db);
        grant_.firstSlot = sel.slot;
        grant_.firstSubchannel = sel.firstSubchannel;
        grant_.numSubchannels = numSubchannels;
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
        SlotIndex next = grant_.nextOccasionAfter(now);
        if (next == SLOTINDEX_NONE) {
            // finite train exhausted (D30): the grant is spent; leftover
            // backlog re-raises mode1BsrPending() -> next BSR/grant cycle
            grantActive_ = false;
            EV << NOW << " NrSlMacUe::ensureTxScheduled - finite grant train exhausted" << endl;
            return;
        }
        scheduleAt(slotGrid_.slotStart(next), txSlotEvent_);
        EV << NOW << " NrSlMacUe::ensureTxScheduled - next TX opportunity at slot " << next
           << " (t=" << slotGrid_.slotStart(next) << ")" << endl;
    }
}

int NrSlMacUe::reservationPeriodMsAt(SlotIndex slot) const
{
    // the SCI's resource reservation field: mode-2 sensers project the
    // transmitter's future occasions from it. Unbounded trains advertise the
    // reservation period; a finite mode-1 train advertises its occasion gap
    // on non-final occasions and 0 on the last (G26 mitigation)
    if (grant_.numOccasions <= 0)
        return periodMs_;
    if (grant_.isLastOccasion(slot))
        return 0;
    return (int)(slotGrid_.getSlotDuration().dbl() * 1000.0 * grant_.occasionGapSlots + 0.5);
}

void NrSlMacUe::handleTxSlot()
{
    SlotIndex slot = slotGrid_.slotIndexAt(NOW);
    EV << NOW << " NrSlMacUe::handleTxSlot - TX opportunity at slot " << slot << endl;

    ASSERT(requestedSdus_ == 0);

    // feedback-mode deadline sweep (D24): missing PSFCH past the deadline
    // resolves per the DTX policy (may queue retransmissions served below)
    if (psfchPeriod_ > 0)
        numDtx_ += harqTx_.processDeadlines(slot, dtxAsAck_, harqMaxRtx_);

    // congestion control (D22): over the CBR level's CR limit, this TX
    // occasion is skipped entirely (retransmissions included); the occasion
    // train stays armed so transmission resumes when the window drains
    const SlCbrLevel *crLevel = slRrc_->getPreconfig().findCbrLevel(lastCbr_);
    if (crLevel != nullptr) {
        double cr = crTracker_.cr(slot, crWindowSlots_, slRrc_->getPreconfig().numSubchannels);
        if (cr > crLevel->crLimit) {
            EV << NOW << " NrSlMacUe::handleTxSlot - CR " << cr << " above the limit "
               << crLevel->crLimit << " (CBR " << lastCbr_ << "), occasion deferred" << endl;
            numCrDeferred_++;
            ensureTxScheduled();
            return;
        }
    }

    // pending HARQ copies (blind or NACK'd) take the TX opportunity before
    // new data (SL-1 simplification: retransmissions ride the same selected
    // resource train, i.e. the next period slots of the grant)
    SlHarqTxEntity::Retx retx;
    if (harqTx_.getNextRetx(retx)) {
        auto slInfo = retx.pdu->getTagForUpdate<SlTxInfoTag>();
        slInfo->setFirstSubchannel(grant_.firstSubchannel);   // current grant resources
        slInfo->setNumSubchannels(grant_.numSubchannels);
        slInfo->setHarqProcId(retx.procId);
        slInfo->setHarqNdi(retx.ndi);
        slInfo->setHarqRv(retx.rv);
        slInfo->setReservationPeriodMs(reservationPeriodMsAt(slot));

        EV << NOW << " NrSlMacUe::handleTxSlot - " << (retx.feedbackMode ? "feedback-driven" : "blind")
           << " retransmission of HARQ proc " << retx.procId << " (rv " << retx.rv << ")" << endl;
        sendLowerPackets(retx.pdu);
        crTracker_.recordTx(slot, grant_.numSubchannels);

        if (retx.feedbackMode) {
            // the copy is acknowledged again: re-arm the feedback wait
            harqTx_.rearmFeedback(retx.procId, slPsfchFeedbackSlot(slot, psfchPeriod_, psfchMinGap_) + 1);
            numFeedbackRetx_++;
        }

        if (allocationMode_ == MODE2)
            sensingDb_.recordUnmonitoredSlot(slot);
        if (allocationMode_ != STATIC && grant_.reselectionCounter > 0 && --grant_.reselectionCounter == 0) {
            if (random_.uniform01() < probResourceKeep_)
                grant_.reselectionCounter = selector_->drawReselectionCounter(periodMs_);
            else
                grantActive_ = false;
        }

        retireOccasionIfLast(slot);

        // new data (and further copies) wait for the next occasion
        for (auto& [cid, connInfo] : connDescOut_) {
            if (!connInfo.buffer->isEmpty()) {
                ensureTxScheduled();
                break;
            }
        }
        if (harqTx_.hasPendingRetx() || harqTx_.hasAwaitingFeedback())
            ensureTxScheduled();
        return;
    }

    // Sidelink LCP (D21): one TB per occasion. Pick the destination owning
    // the highest-priority backlogged connection (lower PQI priority value =
    // more urgent; ties broken by destination id), then fill the TB across
    // that destination's backlogged connections in priority order with
    // SDU requests sized to the remaining TB space. Other destinations
    // compete for whole occasions. PBR/bucket machinery is out of scope.
    const SlPreconfig& cfg = slRrc_->getPreconfig();
    MacNodeId bestDest = NODEID_NONE;
    int bestPrio = std::numeric_limits<int>::max();
    for (auto& [cid, connInfo] : connDescOut_) {
        if (connInfo.buffer->isEmpty())
            continue;
        int prio = cfg.getPqiPriority(connInfo.flowInfo.getSlPqi());
        if (prio < bestPrio || (prio == bestPrio && cid.getNodeId() < bestDest)) {
            bestPrio = prio;
            bestDest = cid.getNodeId();
        }
    }

    if (bestDest != NODEID_NONE) {
        // the selected destination's backlogged connections, by priority
        std::vector<std::pair<int, MacCid>> order;
        for (auto& [cid, connInfo] : connDescOut_)
            if (cid.getNodeId() == bestDest && !connInfo.buffer->isEmpty())
                order.emplace_back(cfg.getPqiPriority(connInfo.flowInfo.getSlPqi()), cid);
        std::sort(order.begin(), order.end());

        int64_t remaining = (int64_t)tbSize_ - (int64_t)MAC_HEADER;
        for (const auto& [prio, cid] : order) {
            if (remaining <= (int64_t)RLC_HEADER_UM)
                break;
            OutgoingConnectionInfo& connInfo = connDescOut_.at(cid);
            int64_t backlog = connInfo.buffer->getQueueOccupancy();
            unsigned int sduSize = (unsigned int)std::min(remaining, backlog + RLC_HEADER_UM);

            EV << NOW << " NrSlMacUe::handleTxSlot - requesting SDU of " << sduSize
               << "B for connection " << cid << " (priority " << prio << ")" << endl;

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
            remaining -= sduSize;
            requestedSdus_++;
        }
    }

    // if nothing was requested, the event goes dormant; new data re-arms it
    // (feedback-mode processes keep occasions alive so deadlines are swept)
    if (requestedSdus_ == 0) {
        if (harqTx_.hasAwaitingFeedback())
            ensureTxScheduled();
        return;
    }

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

    retireOccasionIfLast(slot);
}

void NrSlMacUe::retireOccasionIfLast(SlotIndex slot)
{
    if (grant_.numOccasions > 0 && grant_.isLastOccasion(slot)) {
        grantActive_ = false;
        EV << NOW << " NrSlMacUe::retireOccasionIfLast - last occasion of the finite grant train consumed" << endl;
        // the pending TX of this slot still uses the spent grant's resources;
        // leftover backlog re-raises mode1BsrPending() -> next BSR/grant cycle
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
    pkt->setTimestamp();

    // NOTE: the new-data check must precede the empty-reply check: AM
    // entities announce new data with an empty tag-carrier packet
    // ("AM-NewData"), unlike UM's dup-of-the-PDU convention
    if (pkt->findTag<LteRlcNewDataTag>()) {
        auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
        MacCid cid = MacCid(lteInfo->getDestId(), drbIdToLcid(lteInfo->getDrbId()));
        ASSERT(connDescOut_.find(cid) != connDescOut_.end());
        OutgoingConnectionInfo& connInfo = connDescOut_.at(cid);

        // notification of new data in the RLC buffer: track it in the
        // virtual buffer. RLC-generated control PDUs (AM STATUS/MRW) carry
        // no PdcpTrackingTag; announce them at the AM header size.
        pkt->removeTag<LteRlcNewDataTag>();
        auto pdcpTag = pkt->findTag<PdcpTrackingTag>();
        int64_t announced = (pdcpTag != nullptr) ? pdcpTag->getOriginalPacketLength()
                                                 : std::max((int64_t)pkt->getByteLength(), (int64_t)RLC_HEADER_AM);
        PacketInfo vpkt(announced, pkt->getTimestamp());
        connInfo.buffer->pushBack(vpkt);
        delete pkt;
        return true;
    }

    if (pkt->getBitLength() <= 1) { // empty "no data" reply from RLC
        delete pkt;
        return false;
    }

    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    MacCid cid = MacCid(lteInfo->getDestId(), drbIdToLcid(lteInfo->getDrbId()));
    ASSERT(connDescOut_.find(cid) != connDescOut_.end());

    OutgoingConnectionInfo& connInfo = connDescOut_.at(cid);

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

    // one MAC PDU (TB) per destination, containing the SDUs of ALL of that
    // destination's connections pulled this occasion (D21 grant filling;
    // the container and macPduUnmake handle per-SDU LCIDs)
    std::map<MacNodeId, std::vector<MacCid>> byDest;
    for (auto& [destCid, connInfo] : connDescOut_)
        if (!connInfo.queue->isEmpty())
            byDest[destCid.getNodeId()].push_back(destCid);

    for (auto& [destId, cids] : byDest) {
        OutgoingConnectionInfo& firstConn = connDescOut_.at(cids.front());

        auto macPkt = new Packet("SlMacPdu");
        auto header = makeShared<LteMacPdu>();
        header->setHeaderLength(MAC_HEADER);  // stand-in for the SL-SCH subheader (SRC/DST L2 ids)
        macPkt->insertAtFront(header);

        auto uinfo = macPkt->addTagIfAbsent<UserControlInfo>();
        uinfo->setSourceId(nodeId_);
        uinfo->setDestId(destId);
        uinfo->setDirection(SL);
        uinfo->setFrameType(DATAPKT);
        uinfo->setCarrierFrequency(carrierFrequency_);

        // per-destination fields are shared across the destination's SLRBs
        const FlowDescriptor& flowInfo = firstConn.flowInfo;
        auto slInfo = macPkt->addTagIfAbsent<SlTxInfoTag>();
        slInfo->setSrcL2Id(flowInfo.getSlSrcL2Id());
        slInfo->setDstL2Id(flowInfo.getSlDstL2Id());
        slInfo->setCastType(flowInfo.getSlCastType());
        slInfo->setFirstSubchannel(grant_.firstSubchannel);
        slInfo->setNumSubchannels(grant_.numSubchannels);
        slInfo->setMcs(grant_.mcs);
        slInfo->setReservationPeriodMs(reservationPeriodMsAt(slot));

        macPkt->setTimestamp(NOW);

        for (const MacCid& destCid : cids) {
            OutgoingConnectionInfo& connInfo = connDescOut_.at(destCid);
            while (!connInfo.queue->isEmpty()) {
                auto pkt = check_and_cast<Packet *>(connInfo.queue->popFront());
                drop(pkt);
                pkt->removeTagIfPresent<PdcpTrackingTag>();
                auto macPdu = macPkt->removeAtFront<LteMacPdu>();
                macPdu->pushSdu(pkt, destCid.getLcid());
                macPkt->insertAtFront(macPdu);
            }
        }

        // register the TB with the HARQ entity: PSFCH feedback mode for
        // destinations with feedback configured (D24), blind copies
        // otherwise (WP-F). The stored copy's stale HARQ/grant tag fields
        // are re-stamped at retx time.
        bool feedback = false, nackOnly = false;
        int expectedAcks = 0;
        if (psfchPeriod_ > 0) {
            switch ((SlCastType)flowInfo.getSlCastType()) {
                case SL_UNICAST:
                    feedback = true;
                    expectedAcks = 1;
                    break;
                case SL_GROUPCAST: {
                    const SlrbConfigEntry *slrb = slRrc_->getPreconfig().findSlrbForDstL2Id(flowInfo.getSlDstL2Id());
                    if (slrb != nullptr && slrb->psfchMode == SL_PSFCH_NACK_ONLY) {
                        feedback = true;
                        nackOnly = true;
                    }
                    else if (slrb != nullptr && slrb->psfchMode == SL_PSFCH_ACK_NACK) {
                        feedback = true;
                        expectedAcks = (int)slBinder_->getGroupMembers(flowInfo.getSlDstL2Id()).size() - 1;
                    }
                    break;
                }
                default:
                    break;  // broadcast stays blind
            }
        }

        bool ndi;
        int procId;
        if (feedback) {
            SlotIndex deadline = slPsfchFeedbackSlot(slot, psfchPeriod_, psfchMinGap_) + 1;
            procId = harqTx_.startTbWithFeedback(macPkt, destId, deadline, nackOnly, expectedAcks, ndi);
        }
        else
            procId = harqTx_.startTb(macPkt, blindRetx_, ndi);
        slInfo = macPkt->getTagForUpdate<SlTxInfoTag>();
        slInfo->setHarqProcId(procId);
        slInfo->setHarqNdi(ndi);
        slInfo->setHarqRv(0);

        std::string harqNote = feedback ? (nackOnly ? " (awaiting PSFCH, NACK-only)" : " (awaiting PSFCH)")
                                        : (" (+" + std::to_string(blindRetx_) + " blind copies)");
        EV << NOW << " NrSlMacUe::macPduMake - sending SL MAC PDU (" << macPkt->getByteLength()
           << "B, " << cids.size() << " connection(s)) to slot " << slot << ", dstPid " << destId
           << ", HARQ proc " << procId << harqNote << endl;

        sendLowerPackets(macPkt);
        crTracker_.recordTx(slot, grant_.numSubchannels);
    }

    // keep transmitting on the grant train while backlog, pending copies or
    // awaited feedback remain
    if (harqTx_.hasPendingRetx() || harqTx_.hasAwaitingFeedback())
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

void NrSlMacUe::onCbrUpdated(double cbr)
{
    Enter_Method_Silent("onCbrUpdated()");
    lastCbr_ = cbr;
}

void NrSlMacUe::onPsfchDecoded(MacNodeId fbSender, int harqProcId, bool ack)
{
    Enter_Method_Silent("onPsfchDecoded()");
    EV << NOW << " NrSlMacUe::onPsfchDecoded - " << (ack ? "ACK" : "NACK") << " from node "
       << fbSender << " for HARQ proc " << harqProcId << endl;
    harqTx_.onFeedback(harqProcId, fbSender, ack, harqMaxRtx_);
    if (harqTx_.hasPendingRetx())
        ensureTxScheduled();
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
