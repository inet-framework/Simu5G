//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <assert.h>
#include "simu5g/stack/phy/LtePhyUeD2D.h"

#include "simu5g/stack/rrc/HandoverController.h"
#include "simu5g/stack/phy/packet/LteFeedbackPkt.h"
#include "simu5g/stack/rrc/D2dModeSelectionBase.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

Define_Module(LtePhyUeD2D);
using namespace inet;



void LtePhyUeD2D::initialize(int stage)
{
    LtePhyUe::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        d2dHelper_.setD2dTxPower(par("d2dTxPower"));
        d2dHelper_.setMulticastEnableCaptureEffect(par("d2dMulticastCaptureEffect"));
        d2dHelper_.setMulticastD2DRangeCheckEnabled(par("enableMulticastD2DRangeCheck"));
        d2dHelper_.setMulticastD2DRange(par("multicastD2DRange"));
    }
}

void LtePhyUeD2D::handleSelfMessage(cMessage *msg)
{
    if (msg->isName("d2dDecodingTimer")) {
        // Decode the captured frame (capture effect) and clear the receive buffer.
        d2dHelper_.decodeStoredFrames();

        delete msg;
        d2dDecodingTimer_ = nullptr;
    }
    else
        LtePhyUe::handleSelfMessage(msg);
}

// TODO: ***reorganize*** method
void LtePhyUeD2D::handleAirFrame(cMessage *msg)
{
    LteAirFrame *frame = static_cast<LteAirFrame *>(msg);
    UserControlInfo *lteInfo = new UserControlInfo(frame->getAdditionalInfo());

    EV << "LtePhyUeD2D: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;

    MacNodeId sourceId = lteInfo->getSourceId();
    if (!binder_->nodeExists(sourceId)) {
        EV << "Source has left the simulation." << endl;
        delete msg;
        return;
    }

    GHz carrierFreq = lteInfo->getCarrierFrequency();
    LteChannelModel *channelModel = getChannelModel(carrierFreq);
    if (channelModel == nullptr) {
        EV << "Received packet on carrier frequency not supported by this node. Delete it." << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    // Update coordinates of this user.
    if (lteInfo->getFrameType() == BEACONPKT) {
        // Check if the message is on another carrier frequency
        if (carrierFreq != primaryChannelModel_->getCarrierFrequency()) {
            EV << "Received beacon packet on a different carrier frequency. Delete it." << endl;
            delete lteInfo;
            delete frame;
            return;
        }

        // Check if the message is from a different cellular technology.
        // Note: beacons are the only frames that carry a meaningful isNr flag (it is stamped
        // solely in LtePhyEnb::createBeaconMessage()), and the only true channel broadcasts
        // reaching both radios of a dual-PHY UE; non-beacon frames are technology-routed at
        // the sender. Hence the filter is scoped to beacons, same as in NrPhyUe.
        if (lteInfo->isNr() != isNr_) {
            EV << "Received beacon packet [from NR=" << lteInfo->isNr() << "] from a different radio technology [to NR=" << isNr_ << "]. Delete it." << endl;
            delete lteInfo;
            delete frame;
            return;
        }

        handoverController_->beaconReceived(frame, lteInfo);
        return;
    }

    // Check if the frame is for us (MacNodeId matches or - if this is a multicast communication - enrolled in multicast group).
    if (lteInfo->getDestId() != nodeId_ && !(binder_->isInMulticastGroup(nodeId_, lteInfo->getPacketMulticastGroupId()))) {
        EV << "ERROR: Frame is not for us. Delete it." << endl;
        EV << "Packet Type: " << phyFrameTypeToA((LtePhyFrameType)lteInfo->getFrameType()) << endl;
        EV << "Frame MacNodeId: " << lteInfo->getDestId() << endl;
        EV << "Local MacNodeId: " << nodeId_ << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    /*
     * This could happen if the ue associates with a new master while a packet from the
     * old master is in-flight: the packet is in the air
     * while the ue changes master.
     * Event timing:      TTI x: packet scheduled and sent by the UE (tx time = 1ms)
     *                     TTI x+0.1: ue changes master
     *                     TTI x+1: packet from UE arrives at the old master
     */
    if (lteInfo->getDirection() != D2D && lteInfo->getDirection() != D2D_MULTI && lteInfo->getSourceId() != servingNodeId_) {
        EV << "WARNING: frame from a UE that is leaving this cell (handover): deleted " << endl;
        EV << "Source MacNodeId: " << lteInfo->getSourceId() << endl;
        EV << "UE MacNodeId: " << nodeId_ << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    if (binder_->isInMulticastGroup(nodeId_, lteInfo->getPacketMulticastGroupId())) {
        // HACK: if this is a multicast connection, change the destId of the airframe so that upper layers can handle it.
        lteInfo->setDestId(nodeId_);
    }

    // Send H-ARQ feedback and other control messages up.
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT || lteInfo->getFrameType() == D2DMODESWITCHPKT) {
        EV << "Received control message (H-ARQ feedback / GRANT / RAC / D2D mode switch)." << endl;
        handleControlMsg(frame, lteInfo);
        return;
    }

    // This is a DATA packet.

    if (servingNodeId_ == NODEID_NONE) {
        // UE is not (anymore) associated with any eNB/gNB and all harqBuffers are already deleted.
        // Handing this data packet to the MAC layer will lead to null pointers.
        EV << "LtePhyUeD2D: UE " << nodeId_ << " received data packet while not associated with any base station. (masterId " << servingNodeId_ << "). Drop it." << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    // If the packet is a D2D multicast one, store it and decode it at the end of the TTI.
    if (d2dHelper_.getMulticastEnableCaptureEffect() && binder_->isInMulticastGroup(nodeId_, lteInfo->getPacketMulticastGroupId())) {
        // If not already started, auto-send a message to signal the presence of data to be decoded.
        if (d2dDecodingTimer_ == nullptr) {
            d2dDecodingTimer_ = new cMessage("d2dDecodingTimer");
            d2dDecodingTimer_->setSchedulingPriority(10);          // Last thing to be performed in this TTI.
            scheduleAt(NOW, d2dDecodingTimer_);
        }

        // Store frame, together with related control info.
        frame->setControlInfo(lteInfo);
        d2dHelper_.storeAirFrame(frame);            // Implements the capture effect.

        return;                          // Exit the function, decoding will be done later.
    }

    if ((lteInfo->getUserTxParams()) != nullptr) {
        int cw = lteInfo->getCw();
        if (lteInfo->getUserTxParams()->readCqiVector().size() == 1)
            cw = 0;
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
        if (lteInfo->getDirection() == DL) {
            emit(averageCqiDlSignal_, cqi);
            recordCqi(cqi, DL);
        }
    }

    // Apply decider to received packet.
    bool result = channelModel->isReceptionSuccessful(frame, lteInfo);

    // Update statistics.
    if (result)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    EV << "Handled LteAirframe with ID " << frame->getId() << " with result "
       << (result ? "RECEIVED" : "NOT RECEIVED") << endl;

    auto pkt = check_and_cast<inet::Packet *>(frame->decapsulate());

    // Here frame has to be destroyed since it is no more useful.
    delete frame;

    // Attach the decider result to the packet as control info.
    *(pkt->addTagIfAbsent<UserControlInfo>()) = *lteInfo;
    delete lteInfo;

    pkt->addTagIfAbsent<PhyReceptionInd>()->setDeciderResult(result);

    // Send decapsulated message along with result control info to upperGateOut_.
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}

void LtePhyUeD2D::handleUpperMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto lteInfo = pkt->removeTag<UserControlInfo>();

    GHz carrierFreq = lteInfo->getCarrierFrequency();
    LteChannelModel *channelModel = getChannelModel(carrierFreq);
    if (channelModel == nullptr)
        throw cRuntimeError("LtePhyUeD2D::handleUpperMessage - Carrier frequency [%f] not supported by any channel model", carrierFreq.get());

    if (lteInfo->getFrameType() == DATAPKT && channelModel->recordsUlTransmissionMap()) {
        // Store the RBs used for data transmission to the binder (for UL interference computation).
        RbMap rbMap = lteInfo->getGrantedBlocks();
        Remote antenna = MACRO;  // TODO fix for multi-antenna.
        Direction dir = lteInfo->getDirection();
        binder_->storeUlTransmissionMap(channelModel->getCarrierFrequency(), antenna, rbMap, nodeId_, servingNodeId_, this, dir);
    }

    if (lteInfo->getFrameType() == DATAPKT && lteInfo->getUserTxParams() != nullptr) {
        double cqi = lteInfo->getUserTxParams()->readCqiVector()[lteInfo->getCw()];
        if (lteInfo->getDirection() == UL) {
            emit(averageCqiUlSignal_, cqi);
            recordCqi(cqi, UL);
        }
        else if (lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI)
            emit(averageCqiD2DSignal_, cqi);
    }

    EV << NOW << " LtePhyUeD2D::handleUpperMessage - message from stack" << endl;
    LteAirFrame *frame = nullptr;

    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT) {
        frame = new LteAirFrame("harqFeedback-grant");
    }
    else {
        // Create LteAirFrame and encapsulate the received packet.
        frame = new LteAirFrame("airframe");
    }

    frame->encapsulate(check_and_cast<cPacket *>(msg));

    // Initialize frame fields.

    frame->setSchedulingPriority(airFramePriority_);

    // Set transmission duration according to the numerology.
    NumerologyIndex numerologyIndex = binder_->getNumerologyIndexFromCarrierFreq((lteInfo->getCarrierFrequency()));
    double slotDuration = binder_->getSlotDurationFromNumerologyIndex(numerologyIndex);
    frame->setDuration(slotDuration);

    // Set current position.
    lteInfo->setCoord(getRadioPosition());

    lteInfo->setTxPower(txPower_);
    lteInfo->setD2dTxPower(d2dHelper_.getD2dTxPower());
    frame->setControlInfo(lteInfo.get()->dup());

    EV << "LtePhyUeD2D::handleUpperMessage - " << nodeTypeToA(nodeType_) << " with id " << nodeId_
       << " sending message to the air channel. Dest=" << lteInfo->getDestId() << endl;

    // If this is a multicast/broadcast connection, send the frame to all neighbors in the hearing range.
    // Otherwise, send unicast to the destination.
    if (lteInfo->getDirection() == D2D_MULTI)
        sendMulticast(frame);
    else
        sendUnicast(frame);
}

void LtePhyUeD2D::sendMulticast(LteAirFrame *frame)
{
    UserControlInfo *ci = check_and_cast<UserControlInfo *>(frame->getControlInfo());

    // get the group Id
    MacNodeId groupId = ci->getPacketMulticastGroupId();
    if (groupId == NODEID_NONE)
        throw cRuntimeError("LtePhyUeD2D::sendMulticast - Error. Group ID %d is not valid.", num(groupId));

    // transfer control info into airframe fields
    frame->setAdditionalInfo(*ci);
    delete frame->removeControlInfo();

    // send the frame to nodes belonging to the multicast group only
    for (auto [destId, nodeInfo] : binder_->getNodeInfoMap()) {
        // if the node in the list does not use the same LTE/NR technology of this PHY module, skip it
        if (isNrUe(destId) != isNr_)
            continue;

        if (destId != nodeId_ && binder_->isInMulticastGroup(destId, groupId)) {
            EV << NOW << " LtePhyUeD2D::sendMulticast - node " << destId << " is in the multicast group" << endl;

            // get a pointer to receiving module
            cModule *receiver = nodeInfo.moduleRef;
            LtePhyBase *recvPhy;
            double dist;

            if (d2dHelper_.getMulticastD2DRangeCheckEnabled()) {
                // get the correct PHY layer module
                recvPhy = (isNrUe(destId)) ? check_and_cast<LtePhyBase *>(receiver->getSubmodule("cellularNic")->getSubmodule("nrPhy"))
                                  : check_and_cast<LtePhyBase *>(receiver->getSubmodule("cellularNic")->getSubmodule("phy"));

                dist = recvPhy->getCoord().distance(getRadioPosition());

                if (dist > d2dHelper_.getMulticastD2DRange()) {
                    EV << NOW << " LtePhyUeD2D::sendMulticast - node too far (" << dist << " > " << d2dHelper_.getMulticastD2DRange() << ". skipping transmission" << endl;
                    continue;
                }
            }

            EV << NOW << " LtePhyUeD2D::sendMulticast - sending frame to node " << destId << endl;

            // Create a duplicate frame before sending
            LteAirFrame *frameToSend = frame->dup();
            sendDirect(frameToSend, 0, frame->getDuration(), receiver, getReceiverGateIndex(receiver, isNrUe(destId)));
        }
    }

    // delete the original frame
    delete frame;
}

void LtePhyUeD2D::sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req)
{
    Enter_Method("SendFeedback");
    EV << "LtePhyUeD2D: feedback from Feedback Generator" << endl;

    // Create a feedback packet
    auto fbPkt = makeShared<LteFeedbackPkt>();
    // Set the feedback
    fbPkt->setLteFeedbackDoubleVectorDl(fbDl);
    fbPkt->setLteFeedbackDoubleVectorUl(fbUl);
    fbPkt->setSourceNodeId(nodeId_);

    auto pkt = new Packet("feedback_pkt");
    pkt->insertAtFront(fbPkt);

    UserControlInfo *uinfo = new UserControlInfo();
    uinfo->setSourceId(nodeId_);
    uinfo->setDestId(servingNodeId_);
    uinfo->setFrameType(FEEDBACKPKT);
    // Create LteAirFrame and encapsulate a feedback packet
    LteAirFrame *frame = new LteAirFrame("feedback_pkt");
    frame->encapsulate(check_and_cast<cPacket *>(pkt));
    uinfo->setFeedbackReq(req);
    uinfo->setDirection(UL);
    simtime_t signalLength = TTI;
    uinfo->setTxPower(txPower_);
    uinfo->setD2dTxPower(d2dHelper_.getD2dTxPower());
    // Initialize frame fields

    frame->setSchedulingPriority(airFramePriority_);
    frame->setDuration(signalLength);

    uinfo->setCoord(getRadioPosition());

    lastFeedback_ = NOW;

    // Send one feedback packet for each carrier
    for (auto& cm : channelModel_) {
        GHz carrierFrequency = cm.first;
        LteAirFrame *carrierFrame = frame->dup();
        UserControlInfo *carrierInfo = uinfo->dup();
        carrierInfo->setCarrierFrequency(carrierFrequency);
        carrierFrame->setControlInfo(carrierInfo);

        EV << "LtePhy: " << nodeTypeToA(nodeType_) << " with id "
           << nodeId_ << " sending feedback to the air channel for carrier " << carrierFrequency << endl;
        sendUnicast(carrierFrame);
    }

    delete frame;
    delete uinfo;
}

} //namespace
