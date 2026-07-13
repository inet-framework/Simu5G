//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

// NOTE: this class is the NR counterpart of ~LteMacEnbD2D and its sibling
// implementation. The D2D state and heavy logic live in the shared
// D2dEnbMacHelper, so what remains here is thin dispatch/glue duplicated from
// LteMacEnbD2D.cc. Keep the two in sync.

#include "simu5g/stack/d2d/mac/NrMacGnbD2D.h"
#include "simu5g/stack/mac/LteMacUeD2D.h"
#include "simu5g/stack/phy/packet/LteFeedbackPkt.h"
#include "simu5g/stack/mac/buffer/harq/LteHarqBufferRx.h"
#include "simu5g/stack/mac/buffer/harq_d2d/LteHarqBufferRxD2D.h"
#include "simu5g/stack/mac/amc/AmcPilotD2D.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerEnbUl.h"
#include "simu5g/stack/mac/packet/LteSchedulingGrant.h"
#include "simu5g/stack/mac/conflict_graph/DistanceBasedConflictGraph.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowSignals.h"

namespace simu5g {

Define_Module(NrMacGnbD2D);

using namespace omnetpp;
using namespace inet;



NrMacGnbD2D::NrMacGnbD2D() : d2dEnbHelper_(this)
{
}

void NrMacGnbD2D::initialize(int stage)
{
    NrMacGnb::initialize(stage);
    if (stage == INITSTAGE_PHYSICAL_ENVIRONMENT) {
        bool usePreconfiguredTxParams = par("usePreconfiguredTxParams");
        Cqi d2dCqi = par("d2dCqi");
        if (usePreconfiguredTxParams)
            check_and_cast<AmcPilotD2D *>(amc_->getPilot())->setPreconfiguredTxParams(d2dCqi);

        d2dEnbHelper_.setMsHarqInterrupt(par("msHarqInterrupt").boolValue());
        d2dEnbHelper_.setMsClearRlcBuffer(par("msClearRlcBuffer").boolValue());
    }
    else if (stage == INITSTAGE_LAST) { // be sure that all UEs have been initialized
        d2dEnbHelper_.setReuseD2D(par("reuseD2D"));
        d2dEnbHelper_.setReuseD2DMulti(par("reuseD2DMulti"));

        if (d2dEnbHelper_.getReuseD2D() || d2dEnbHelper_.getReuseD2DMulti()) {
            d2dEnbHelper_.setConflictGraphUpdatePeriod(par("conflictGraphUpdatePeriod"));

            d2dEnbHelper_.createDistanceBasedConflictGraph(binder_, par("conflictGraphThreshold"),
                    par("conflictGraphD2DInterferenceRadius"), par("conflictGraphD2DMultiTxRadius"), par("conflictGraphD2DMultiInterferenceRadius"));

            scheduleAt(NOW + 0.05, new cMessage("updateConflictGraph"));
        }

    }
    else if (stage == INITSTAGE_SIMU5G_AMC_SETUP) {
        bool usePreconfiguredTxParams = par("usePreconfiguredTxParams");
        Cqi d2dCqi = par("d2dCqi");
        if (usePreconfiguredTxParams)
            check_and_cast<AmcPilotD2D *>(amc_->getPilot())->setPreconfiguredTxParams(d2dCqi);

        d2dEnbHelper_.setMsHarqInterrupt(par("msHarqInterrupt").boolValue());
        d2dEnbHelper_.setMsClearRlcBuffer(par("msClearRlcBuffer").boolValue());
    }
}

void NrMacGnbD2D::macHandleFeedbackPkt(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    auto fb = pkt->peekAtFront<LteFeedbackPkt>();
    auto lteInfo = pkt->getTag<UserControlInfo>();

    std::map<MacNodeId, LteFeedbackDoubleVector> fbMapD2D = fb->getLteFeedbackDoubleVectorD2D();

    // skip if no D2D CQI has been reported
    if (!fbMapD2D.empty()) {
        //get Source Node Id<
        MacNodeId id = fb->getSourceNodeId();

        // extract feedback for D2D links
        for (const auto& mapIt : fbMapD2D) {
            MacNodeId peerId = mapIt.first;
            for (const auto& it : mapIt.second) {
                for (const auto& jt : it) {
                    if (!jt.isEmptyFeedback()) {
                        amc_->pushFeedbackD2D(id, jt, peerId, lteInfo->getCarrierFrequency());
                    }
                }
            }
        }
    }
    NrMacGnb::macHandleFeedbackPkt(pkt);
}

void NrMacGnbD2D::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage() && msg->isName("D2DModeSwitchNotification")) {
        cPacket *pkt = check_and_cast<cPacket *>(msg);
        d2dEnbHelper_.macHandleD2DModeSwitch(pkt);
        delete pkt;
    }
    else if (msg->isSelfMessage() && msg->isName("updateConflictGraph")) {
        // compute conflict graph for resource allocation
        d2dEnbHelper_.computeConflictGraph();

        scheduleAt(NOW + d2dEnbHelper_.getConflictGraphUpdatePeriod(), msg);
    }
    else
        NrMacGnb::handleMessage(msg);
}

void NrMacGnbD2D::handleSelfMessage()
{
    // Call the eNodeB main loop
    NrMacGnb::handleSelfMessage();
}

void NrMacGnbD2D::macPduUnmake(cPacket *cpkt)
{
    auto pkt = check_and_cast<Packet *>(cpkt);
    auto macPdu = pkt->removeAtFront<LteMacPdu>();
    auto userInfo = pkt->getTag<UserControlInfo>();

    // Notify the packet flow manager about the successful arrival of a TB from a UE.
    // From ETSI TS 138314 V16.0.0 (2020-07)
    if (hasListeners(ulMacPduArrivedSignal_)) {
        GrantSignalInfo ulInfo(userInfo->getSourceId(), userInfo->getGrantId());
        emit(ulMacPduArrivedSignal_, &ulInfo);
    }

    while (macPdu->hasSdu()) {
        // Extract and send SDU
        LogicalCid lcid;
        auto upPkt = macPdu->popSdu(lcid);
        take(upPkt);

        EV << "NrMacGnbD2D: pduUnmaker extracted SDU" << endl;

        MacNodeId senderId = userInfo->getSourceId();
        MacCid cid = MacCid(senderId, lcid);
        ASSERT(connDescIn_.find(cid) != connDescIn_.end());
        *upPkt->addTag<FlowControlInfo>() = connDescIn_[cid].toFlowControlInfo();

        EV << "NrMacGnbD2D: Lcid --->"<< (int)lcid << " Cid: " << cid <<endl;

        sendUpperPackets(upPkt);
    }

    while (macPdu->hasCe()) {
        // Extract CE
        // TODO: see if for cid or lcid
        MacBsr *bsr = check_and_cast<MacBsr *>(macPdu->popCe());
        auto lteInfo = pkt->getTag<UserControlInfo>();
        LogicalCid lcid = lteInfo->getPacketLcid();  // one of SHORT_BSR or D2D_MULTI_SHORT_BSR

        MacCid cid = MacCid(lteInfo->getSourceId(), lcid); // this way, different connections from the same UE (e.g. one UL and one D2D)
                                                               // obtain different CIDs. With the inverse operation, you can get
                                                               // the LCID and discover if the connection is UL or D2D
        bufferizeBsr(bsr, cid);
    }
    pkt->insertAtFront(macPdu);

    delete pkt;
}

void NrMacGnbD2D::sendGrants(std::map<GHz, LteMacScheduleList> *scheduleList)
{
    EV << NOW << "NrMacGnbD2D::sendGrants " << endl;

    for (auto& [carrierFreq, carrierScheduleList] : *scheduleList) {
        while (!carrierScheduleList.empty()) {
            LteMacScheduleList::iterator it, ot;
            it = carrierScheduleList.begin();

            Codeword cw = it->first.second;
            Codeword otherCw = MAX_CODEWORDS - cw;
            MacCid cid = it->first.first;
            LogicalCid lcid = cid.getLcid();
            MacNodeId nodeId = cid.getNodeId();
            unsigned int granted = it->second;
            unsigned int codewords = 0;

            // removing visited element from scheduleList.
            carrierScheduleList.erase(it);

            if (granted > 0) {
                // increment number of allocated Cw
                ++codewords;
            }
            else {
                // active cw becomes the "other one"
                cw = otherCw;
            }

            std::pair<MacCid, Codeword> otherPair(MacCid(nodeId, LogicalCid(0)), otherCw);

            if ((ot = (carrierScheduleList.find(otherPair))) != (carrierScheduleList.end())) {
                // increment number of allocated Cw
                ++codewords;

                // removing visited element from scheduleList.
                carrierScheduleList.erase(ot);
            }

            if (granted == 0)
                continue; // avoiding transmission of 0 grant (0 grant should not be created)

            EV << NOW << " NrMacGnbD2D::sendGrants Node[" << getMacNodeId() << "] - "
               << granted << " blocks to grant for user " << nodeId << " on "
               << codewords << " codewords. CW[" << cw << "\\" << otherCw << "] carrier[" << carrierFreq << "]" << endl;

            // get the direction of the grant, depending on which connection has been scheduled by the eNB
            Direction dir = directionFromBsrLcid(lcid, UL);

            // TODO Grant is set aperiodic as default
            // TODO: change to tag instead of header
            auto pkt = new Packet("LteGrant");
            auto grant = makeShared<LteSchedulingGrant>();
            grant->setDirection(dir);
            grant->setCodewords(codewords);

            // set total granted blocks
            grant->setTotalGrantedBlocks(granted);
            grant->setChunkLength(b(1));

            pkt->addTagIfAbsent<UserControlInfo>()->setSourceId(getMacNodeId());
            pkt->addTagIfAbsent<UserControlInfo>()->setDestId(nodeId);
            pkt->addTagIfAbsent<UserControlInfo>()->setFrameType(GRANTPKT);
            pkt->addTagIfAbsent<UserControlInfo>()->setCarrierFrequency(carrierFreq);

            const UserTxParams& ui = getAmc()->computeTxParams(nodeId, dir, carrierFreq);
            UserTxParams *txPara = new UserTxParams(ui);
            // FIXME: possible memory leak
            grant->setUserTxParams(txPara);

            // acquiring remote antennas set from user info
            const std::set<Remote>& antennas = ui.readAntennaSet();

            // get bands for this carrier
            const unsigned int firstBand = cellInfo_->getCarrierStartingBand(carrierFreq);
            const unsigned int lastBand = cellInfo_->getCarrierLastBand(carrierFreq);

            //  HANDLE MULTICW
            for ( ; cw < codewords; ++cw) {
                unsigned int grantedBytes = 0;

                for (Band b = firstBand; b <= lastBand; ++b) {
                    unsigned int bandAllocatedBlocks = 0;
                    for (const auto& antenna : antennas) {
                        bandAllocatedBlocks += enbSchedulerUl_->readPerUeAllocatedBlocks(nodeId, antenna, b);
                    }
                    grantedBytes += amc_->computeBytesOnNRbs(nodeId, b, cw, bandAllocatedBlocks, dir, carrierFreq);
                }

                grant->setGrantedCwBytes(cw, grantedBytes);
                EV << NOW << " NrMacGnbD2D::sendGrants - granting " << grantedBytes << " on cw " << cw << endl;
            }
            RbMap map;

            enbSchedulerUl_->readRbOccupation(nodeId, carrierFreq, map);

            grant->setGrantedBlocks(map);

            /*
             * @author Alessandro Noferi
             * Notify the packet flow manager about the successful arrival of a TB from a UE.
             * From ETSI TS 138314 V16.0.0 (2020-07)
             *   tSched: the point in time when the UL MAC SDU i is scheduled as
             *   per the scheduling grant provided
             */
            if (hasListeners(grantSentSignal_)) {
                GrantSignalInfo grantInfo(nodeId, grant->getGrantId());
                emit(grantSentSignal_, &grantInfo);
            }

            // send grant to PHY layer
            pkt->insertAtFront(grant);
            sendLowerPackets(pkt);
        }
    }
}

HarqBuffersMirrorD2D *NrMacGnbD2D::getHarqBuffersMirrorD2D(GHz carrierFrequency)
{
    return d2dEnbHelper_.getHarqBuffersMirrorD2D(carrierFrequency);
}

void NrMacGnbD2D::deleteQueues(MacNodeId nodeId)
{
    NrMacGnb::deleteQueues(nodeId);
    deleteHarqBuffersMirrorD2D(nodeId);
}

void NrMacGnbD2D::deleteHarqBuffersMirrorD2D(MacNodeId nodeId)
{
    d2dEnbHelper_.deleteHarqBuffersMirrorD2D(nodeId);
}

void NrMacGnbD2D::deleteHarqBuffersMirrorD2D(MacNodeId txPeer, MacNodeId rxPeer)
{
    d2dEnbHelper_.deleteHarqBuffersMirrorD2D(txPeer, rxPeer);
}

void NrMacGnbD2D::sendModeSwitchNotification(MacNodeId srcId, MacNodeId dstId, LteD2DMode oldMode, LteD2DMode newMode)
{
    Enter_Method_Silent("sendModeSwitchNotification");

    EV << NOW << " NrMacGnbD2D::sendModeSwitchNotification - " << srcId << " --> " << dstId << " going from " << d2dModeToA(oldMode) << " to " << d2dModeToA(newMode) << endl;

    // send switch notification to both the tx and rx side of the flow

    auto pktTx = new inet::Packet("D2DModeSwitchNotification");

    auto switchPktTx = makeShared<D2DModeSwitchNotification>();
    switchPktTx->setTxSide(true);
    switchPktTx->setPeerId(dstId);
    switchPktTx->setOldMode(oldMode);
    switchPktTx->setNewMode(newMode);
    switchPktTx->setInterruptHarq(d2dEnbHelper_.getMsHarqInterrupt());
    switchPktTx->setClearRlcBuffer(d2dEnbHelper_.getMsClearRlcBuffer());

    pktTx->addTagIfAbsent<UserControlInfo>()->setSourceId(nodeId_);
    pktTx->addTagIfAbsent<UserControlInfo>()->setDestId(srcId);
    pktTx->addTagIfAbsent<UserControlInfo>()->setFrameType(D2DMODESWITCHPKT);

    pktTx->insertAtFront(switchPktTx);
    auto switchPktTx_local = pktTx->dup();
    sendLowerPackets(pktTx);

    auto pktRx = new inet::Packet("D2DModeSwitchNotification");
    auto switchPktRx = makeShared<D2DModeSwitchNotification>();
    switchPktRx->setTxSide(false);
    switchPktRx->setPeerId(srcId);
    switchPktRx->setOldMode(oldMode);
    switchPktRx->setNewMode(newMode);
    switchPktRx->setInterruptHarq(d2dEnbHelper_.getMsHarqInterrupt());
    switchPktRx->setClearRlcBuffer(d2dEnbHelper_.getMsClearRlcBuffer());

    pktRx->addTagIfAbsent<UserControlInfo>()->setSourceId(nodeId_);
    pktRx->addTagIfAbsent<UserControlInfo>()->setDestId(dstId);
    pktRx->addTagIfAbsent<UserControlInfo>()->setFrameType(D2DMODESWITCHPKT);
    pktRx->insertAtFront(switchPktRx);

    auto switchPktRx_local = pktRx->dup();
    sendLowerPackets(pktRx);

    scheduleAt(NOW + TTI, switchPktTx_local);
    scheduleAt(NOW + TTI, switchPktRx_local);
}

void NrMacGnbD2D::flushHarqBuffers()
{
    for (auto& mit : harqTxBuffers_) {
        for (auto& it : mit.second)
            it.second->sendSelectedDown();
    }

    // flush mirror buffer
    for (auto& mirr_mit : d2dEnbHelper_.getHarqBuffersMirrorD2DMap()) {
        for (auto& mirr_it : mirr_mit.second)
            mirr_it.second->markSelectedAsWaiting();
    }
}

/*
 * Lower layer handler
 */
LteHarqBufferRx *NrMacGnbD2D::createRxHarqBuffer(MacNodeId src, const UserControlInfo *userInfo)
{
    Direction dir = (Direction)userInfo->getDirection();
    if (dir == D2D || dir == D2D_MULTI)
        return new LteHarqBufferRxD2D(harqProcesses_, this, binder_, src, (dir == D2D_MULTI));
    return NrMacGnb::createRxHarqBuffer(src, userInfo);
}

void NrMacGnbD2D::fromPhy(cPacket *pktAux)
{
    // TODO: harq test (commenting fromPhy: it has only to pass PDUs to the proper RX buffer and
    // to manage H-ARQ feedback)
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto userInfo = pkt->getTag<UserControlInfo>();
    if (userInfo->getFrameType() == HARQPKT) {
        MacNodeId src = userInfo->getSourceId();
        GHz carrierFrequency = userInfo->getCarrierFrequency();

        // this feedback refers to a mirrored H-ARQ buffer
        auto hfbpkt = pkt->peekAtFront<LteHarqFeedback>();
        if (!hfbpkt->getD2dFeedback()) { // this is not a mirror feedback
            LteMacBase::fromPhy(pkt);
            return;
        }

        // H-ARQ feedback, send it to the mirror buffer of the D2D pair
        auto mfbpkt = pkt->peekAtFront<LteHarqFeedbackMirror>();
        MacNodeId d2dSender = mfbpkt->getD2dSenderId();
        MacNodeId d2dReceiver = mfbpkt->getD2dReceiverId();
        D2DPair pair(d2dSender, d2dReceiver);
        auto& harqBuffersMirrorD2D = d2dEnbHelper_.getHarqBuffersMirrorD2DMap();
        HarqBuffersMirrorD2D::iterator hit = harqBuffersMirrorD2D[carrierFrequency].find(pair);
        EV << NOW << "NrMacGnbD2D::fromPhy - node " << nodeId_ << " Received HARQ Feedback pkt (mirrored)" << endl;
        if (hit == harqBuffersMirrorD2D[carrierFrequency].end()) {
            // if feedback arrives, a buffer should exist (unless it is a handover scenario
            // where the HARQ buffer was deleted but feedback was in transit)
            // this case must be taken care of
            if (binder_->hasUeHandoverTriggered(src))
                return;

            // create buffer
            LteHarqBufferMirrorD2D *hb = new LteHarqBufferMirrorD2D((unsigned int)harqProcesses_, (unsigned char)par("maxHarqRtx"), this);
            harqBuffersMirrorD2D[carrierFrequency][pair] = hb;
            hb->receiveHarqFeedback(pkt);
        }
        else {
            hit->second->receiveHarqFeedback(pkt);
        }
    }
    else {
        LteMacBase::fromPhy(pkt);
    }
}

} //namespace
