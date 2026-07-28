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

#ifndef STACK_D2D_PHY_D2DUEPHY_H_
#define STACK_D2D_PHY_D2DUEPHY_H_

#include "simu5g/stack/phy/LtePhyUe.h"
#include "simu5g/stack/phy/packet/LteFeedbackPkt.h"
#include "simu5g/stack/rrc/HandoverController.h"
#include "simu5g/stack/d2d/phy/D2dUePhyHelper.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace omnetpp;

/**
 * CRTP mixin that adds D2D support to a UE PHY.
 *
 * The core UE PHYs (LtePhyUe and its NR subclass NrPhyUe) carry no D2D code;
 * this mixin layers the D2D UE-PHY logic (D2D Tx power, D2D CQI accounting,
 * one-to-many D2D transmission, the D2D-multicast capture effect) on top of
 * either of them, so the LTE and NR D2D PHYs share one implementation. The
 * capture-effect machinery lives in D2dUePhyHelper; the mixin holds the
 * module-side glue the two leaf classes used to duplicate. It has native
 * protected access to the Base internals it needs.
 *
 * The concrete Define_Module'd PHYs are:
 *   LtePhyUeD2D = D2dUePhy<LtePhyUe>
 *   NrPhyUeD2D  = D2dUePhy<NrPhyUe>
 * (NrPhyUeD2D must remain an is-a NrPhyUe: the dynamic_cast<NrPhyUe *> in
 * HandoverController is the dual-stack discriminator.)
 *
 * Note: handleAirFrame() fully replaces the Base implementation (both leaves
 * carried the same, NR-shaped body); the other overrides extend Base behavior
 * and chain up to it.
 */
template<class Base>
class D2dUePhy : public Base
{
  protected:
    // holds the D2D-specific UE-PHY state and logic:
    // D2D Tx power and the D2D-multicast capture-effect machinery
    D2dUePhyHelper d2dHelper_{this};

    // timer for triggering decoding at the end of the TTI. Started when the first
    // airframe is received. The self-message stays in the module (which owns it);
    // the captured frames it decodes live in d2dHelper_.
    cMessage *d2dDecodingTimer_ = nullptr;

    void initialize(int stage) override;
    void handleAirFrame(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg) override;

    // ---- outgoing-frame seams (replace the historical handleUpperMessage copy) ----

    /// the D2D UE PHY performs no serving-cell check on outgoing frames
    /// (D2D/D2D_MULTI frames legitimately target peers)
    void validateOutgoingFrame(const UserControlInfo *info) override {}

    /// D2D CQI accounting for outgoing data packets
    void recordExtraTxCqi(double cqi, const UserControlInfo *info) override
    {
        if (info->getDirection() == D2D || info->getDirection() == D2D_MULTI)
            this->emit(this->averageCqiD2DSignal_, cqi);
    }

    /// keep the historical D2D frame naming (all control frames named "harqFeedback-grant")
    const char *airFrameNameFor(const UserControlInfo *info) override
    {
        switch (info->getFrameType()) {
            case HARQPKT:
            case GRANTPKT:
            case RACPKT: return "harqFeedback-grant";
            default: return "airframe";
        }
    }

    void stampExtraTxControlInfo(UserControlInfo *info) override
    {
        info->setD2dTxPower(d2dHelper_.getD2dTxPower());
    }

    /// one-to-many D2D transmissions go out via sendDirect to all group members
    void transmitFrame(LteAirFrame *frame, const UserControlInfo *info) override
    {
        if (info->getDirection() == D2D_MULTI)
            sendMulticast(frame);
        else
            this->sendUnicast(frame);
    }

    /**
     * Sends a frame to the UEs registered to the multicast group indicated in
     * the frame (optionally skipping receivers beyond multicastD2DRange).
     * Frames are sent with zero transmission delay. D2D-specific: only the
     * D2D UE PHY originates one-to-many D2D transmissions.
     */
    void sendMulticast(LteAirFrame *frame);

  public:
    void sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req) override;

    double getTxPwr(Direction dir = UNKNOWN_DIRECTION) override
    {
        if (dir == D2D)
            return d2dHelper_.getD2dTxPower();
        return this->txPower_;
    }
};

template<class Base>
void D2dUePhy<Base>::initialize(int stage)
{
    Base::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        d2dHelper_.setD2dTxPower(this->par("d2dTxPower"));
        d2dHelper_.setMulticastEnableCaptureEffect(this->par("d2dMulticastCaptureEffect"));
        d2dHelper_.setMulticastD2DRangeCheckEnabled(this->par("enableMulticastD2DRangeCheck"));
        d2dHelper_.setMulticastD2DRange(this->par("multicastD2DRange"));
    }
}

template<class Base>
void D2dUePhy<Base>::handleSelfMessage(cMessage *msg)
{
    if (msg->isName("d2dDecodingTimer")) {
        // Decode the captured frame (capture effect) and clear the receive buffer.
        d2dHelper_.decodeStoredFrames();

        delete msg;
        d2dDecodingTimer_ = nullptr;
    }
    else
        Base::handleSelfMessage(msg);
}

// TODO: ***reorganize*** method
template<class Base>
void D2dUePhy<Base>::handleAirFrame(cMessage *msg)
{
    LteAirFrame *frame = static_cast<LteAirFrame *>(msg);
    UserControlInfo *lteInfo = new UserControlInfo(frame->getAdditionalInfo());

    EV << "D2dUePhy: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;

    MacNodeId sourceId = lteInfo->getSourceId();
    if (!this->binder_->nodeExists(sourceId)) {
        EV << "Source has left the simulation." << endl;
        delete msg;
        return;
    }

    GHz carrierFreq = lteInfo->getCarrierFrequency();
    LteChannelModel *channelModel = this->getChannelModel(carrierFreq);
    if (channelModel == nullptr) {
        EV << "Received packet on carrier frequency not supported by this node. Delete it." << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    // Update coordinates of this user.
    if (lteInfo->getFrameType() == BEACONPKT) {
        // Check if the message is on another carrier frequency
        if (carrierFreq != this->primaryChannelModel_->getCarrierFrequency()) {
            EV << "Received beacon packet on a different carrier frequency. Delete it." << endl;
            delete lteInfo;
            delete frame;
            return;
        }

        // Check if the message is from a different cellular technology.
        // Note: beacons are the only frames that carry a meaningful isNr flag (it is stamped
        // solely in LtePhyEnb::createBeaconMessage()), and the only true channel broadcasts
        // reaching both radios of a dual-PHY UE; non-beacon frames are technology-routed at
        // the sender.
        if (lteInfo->isNr() != this->isNr_) {
            EV << "Received beacon packet [from NR=" << lteInfo->isNr() << "] from a different radio technology [to NR=" << this->isNr_ << "]. Delete it." << endl;
            delete lteInfo;
            delete frame;
            return;
        }

        this->handoverController_->beaconReceived(frame, lteInfo);
        return;
    }

    // Check if the frame is for us (MacNodeId matches or - if this is a multicast communication - enrolled in multicast group).
    if (lteInfo->getDestId() != this->nodeId_ && !(this->binder_->isInMulticastGroup(this->nodeId_, lteInfo->getPacketMulticastGroupId()))) {
        EV << "ERROR: Frame is not for us. Delete it." << endl;
        EV << "Packet Type: " << phyFrameTypeToA((LtePhyFrameType)lteInfo->getFrameType()) << endl;
        EV << "Frame MacNodeId: " << lteInfo->getDestId() << endl;
        EV << "Local MacNodeId: " << this->nodeId_ << endl;
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
    if (lteInfo->getDirection() != D2D && lteInfo->getDirection() != D2D_MULTI && lteInfo->getSourceId() != this->servingNodeId_) {
        EV << "WARNING: frame from a UE that is leaving this cell (handover): deleted " << endl;
        EV << "Source MacNodeId: " << lteInfo->getSourceId() << endl;
        EV << "UE MacNodeId: " << this->nodeId_ << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    if (this->binder_->isInMulticastGroup(this->nodeId_, lteInfo->getPacketMulticastGroupId())) {
        // HACK: if this is a multicast connection, change the destId of the airframe so that upper layers can handle it.
        lteInfo->setDestId(this->nodeId_);
    }

    // Send H-ARQ feedback and other control messages up.
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT || lteInfo->getFrameType() == D2DMODESWITCHPKT) {
        EV << "Received control message (H-ARQ feedback / GRANT / RAC / D2D mode switch)." << endl;
        this->handleControlMsg(frame, lteInfo);
        return;
    }

    // This is a DATA packet.

    if (this->servingNodeId_ == NODEID_NONE) {
        // UE is not (anymore) associated with any eNB/gNB and all harqBuffers are already deleted.
        // Handing this data packet to the MAC layer will lead to null pointers.
        EV << "D2dUePhy: UE " << this->nodeId_ << " received data packet while not associated with any base station. (masterId " << this->servingNodeId_ << "). Drop it." << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    // If the packet is a D2D multicast one, store it and decode it at the end of the TTI.
    if (d2dHelper_.getMulticastEnableCaptureEffect() && this->binder_->isInMulticastGroup(this->nodeId_, lteInfo->getPacketMulticastGroupId())) {
        // If not already started, auto-send a message to signal the presence of data to be decoded.
        if (d2dDecodingTimer_ == nullptr) {
            d2dDecodingTimer_ = new cMessage("d2dDecodingTimer");
            d2dDecodingTimer_->setSchedulingPriority(10);          // Last thing to be performed in this TTI.
            this->scheduleAt(NOW, d2dDecodingTimer_);
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
            this->emit(this->averageCqiDlSignal_, cqi);
            this->recordCqi(cqi, DL);
        }
    }

    // Apply decider to received packet.
    bool result = channelModel->isReceptionSuccessful(frame, lteInfo);

    // Update statistics.
    if (result)
        this->numAirFrameReceived_++;
    else
        this->numAirFrameNotReceived_++;

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
    this->send(pkt, this->upperGateOut_);

    if (getEnvir()->isGUI())
        this->updateDisplayString();
}

template<class Base>
void D2dUePhy<Base>::sendMulticast(LteAirFrame *frame)
{
    UserControlInfo *ci = check_and_cast<UserControlInfo *>(frame->getControlInfo());

    // get the group Id
    MacNodeId groupId = ci->getPacketMulticastGroupId();
    if (groupId == NODEID_NONE)
        throw cRuntimeError("D2dUePhy::sendMulticast - Error. Group ID %d is not valid.", num(groupId));

    // transfer control info into airframe fields
    frame->setAdditionalInfo(*ci);
    delete frame->removeControlInfo();

    // send the frame to nodes belonging to the multicast group only
    for (auto [destId, nodeInfo] : this->binder_->getNodeInfoMap()) {
        // if the node in the list does not use the same LTE/NR technology of this PHY module, skip it
        if (isNrUe(destId) != this->isNr_)
            continue;

        if (destId != this->nodeId_ && this->binder_->isInMulticastGroup(destId, groupId)) {
            EV << NOW << " D2dUePhy::sendMulticast - node " << destId << " is in the multicast group" << endl;

            // get a pointer to receiving module
            cModule *receiver = nodeInfo.moduleRef;
            LtePhyBase *recvPhy;
            double dist;

            if (d2dHelper_.getMulticastD2DRangeCheckEnabled()) {
                // get the correct PHY layer module
                recvPhy = (isNrUe(destId)) ? check_and_cast<LtePhyBase *>(receiver->getSubmodule("cellularNic")->getSubmodule("nrPhy"))
                                  : check_and_cast<LtePhyBase *>(receiver->getSubmodule("cellularNic")->getSubmodule("phy"));

                dist = recvPhy->getCoord().distance(this->getRadioPosition());

                if (dist > d2dHelper_.getMulticastD2DRange()) {
                    EV << NOW << " D2dUePhy::sendMulticast - node too far (" << dist << " > " << d2dHelper_.getMulticastD2DRange() << ". skipping transmission" << endl;
                    continue;
                }
            }

            EV << NOW << " D2dUePhy::sendMulticast - sending frame to node " << destId << endl;

            // Create a duplicate frame before sending
            LteAirFrame *frameToSend = frame->dup();
            this->sendDirect(frameToSend, 0, frame->getDuration(), receiver, this->getReceiverGateIndex(receiver, destId));
        }
    }

    // delete the original frame
    delete frame;
}

template<class Base>
void D2dUePhy<Base>::sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req)
{
    Enter_Method("SendFeedback");
    EV << "D2dUePhy: feedback from Feedback Generator" << endl;

    // Create a feedback packet
    auto fbPkt = inet::makeShared<LteFeedbackPkt>();
    // Set the feedback
    fbPkt->setLteFeedbackDoubleVectorDl(fbDl);
    fbPkt->setLteFeedbackDoubleVectorUl(fbUl);
    fbPkt->setSourceNodeId(this->nodeId_);

    auto pkt = new inet::Packet("feedback_pkt");
    pkt->insertAtFront(fbPkt);

    UserControlInfo *uinfo = new UserControlInfo();
    uinfo->setSourceId(this->nodeId_);
    uinfo->setDestId(this->servingNodeId_);
    uinfo->setFrameType(FEEDBACKPKT);
    // Create LteAirFrame and encapsulate a feedback packet
    LteAirFrame *frame = new LteAirFrame("feedback_pkt");
    frame->encapsulate(check_and_cast<cPacket *>(pkt));
    uinfo->setFeedbackReq(req);
    uinfo->setDirection(UL);
    simtime_t signalLength = TTI;
    uinfo->setTxPower(this->txPower_);
    uinfo->setD2dTxPower(d2dHelper_.getD2dTxPower());
    // Initialize frame fields

    frame->setSchedulingPriority(this->airFramePriority_);
    frame->setDuration(signalLength);

    uinfo->setCoord(this->getRadioPosition());

    this->lastFeedback_ = NOW;

    // Send one feedback packet for each carrier
    for (auto& cm : this->channelModel_) {
        GHz carrierFrequency = cm.first;
        LteAirFrame *carrierFrame = frame->dup();
        UserControlInfo *carrierInfo = uinfo->dup();
        carrierInfo->setCarrierFrequency(carrierFrequency);
        carrierFrame->setControlInfo(carrierInfo);

        EV << "D2dUePhy: " << nodeTypeToA(this->nodeType_) << " with id "
           << this->nodeId_ << " sending feedback to the air channel for carrier " << carrierFrequency << endl;
        this->sendUnicast(carrierFrame);
    }

    delete frame;
    delete uinfo;
}

} //namespace

#endif
