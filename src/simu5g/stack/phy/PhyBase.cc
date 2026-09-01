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

#include "simu5g/stack/phy/PhyBase.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/stack/mac/LteMacEnb.h"

namespace simu5g {

using namespace omnetpp;

short PhyBase::airFramePriority_ = 10;

//Statistics
simsignal_t PhyBase::averageCqiDlSignal_ = registerSignal("averageCqiDl");
simsignal_t PhyBase::averageCqiUlSignal_ = registerSignal("averageCqiUl");


void PhyBase::initialize(int stage)
{
    ChannelAccess::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
        // get gate ids
        upperGateIn_ = findGate("upperGateIn");
        upperGateOut_ = findGate("upperGateOut");
        radioInGate_ = findGate("radioIn");

        // Initialize and watch statistics
        ueTxPower_ = par("ueTxPower");
        eNodeBtxPower_ = par("eNodeBTxPower");
        microTxPower_ = par("microTxPower");
        isNr_ = par("isNr");

        WATCH(numAirFrameReceived_);
        WATCH(numAirFrameNotReceived_);
    }
    else if (stage == INITSTAGE_SIMU5G_REGISTRATIONS2) {
        initializeRadio();
    }
}

void PhyBase::handleMessage(cMessage *msg)
{
    EV << "PhyBase::handleMessage - new message received" << endl;

    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    // AirFrame
    else if (msg->getArrivalGate()->getId() == radioInGate_) {
        handleAirFrame(msg);
    }
    // message from stack
    else if (msg->getArrivalGate()->getId() == upperGateIn_) {
        handleUpperMessage(msg);
    }
    // unknown message
    else {
        EV << "Unknown message received." << endl;
        delete msg;
    }
}

void PhyBase::handleControlMsg(LteAirFrame *frame,
        UserControlInfo *userInfo)
{
    auto pkt = check_and_cast<inet::Packet *>(frame->decapsulate());
    delete frame;
    *(pkt->addTagIfAbsent<UserControlInfo>()) = *userInfo;
    delete userInfo;
    send(pkt, upperGateOut_);
}

void PhyBase::sendDecodedPacketUp(inet::Packet *pkt, bool receptionSuccessful)
{
    // Update statistics
    if (receptionSuccessful)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    pkt->addTagIfAbsent<PhyReceptionInd>()->setDeciderResult(receptionSuccessful);

    // Send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}

void PhyBase::handleUpperMessage(cMessage *msg)
{
    EV << "Phy: message from stack" << endl;

    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto lteInfo = pkt->removeTag<UserControlInfo>();

    LteAirFrame *frame = new LteAirFrame(airFrameNameFor(lteInfo.get()));

    frame->encapsulate(check_and_cast<cPacket *>(msg));

    // initialize frame fields
    frame->setSchedulingPriority(airFramePriorityFor(lteInfo.get()));

    // set transmission duration according to the numerology
    NumerologyIndex numerologyIndex = binder_->getNumerologyIndexFromCarrierFreq(lteInfo->getCarrierFrequency());
    double slotDuration = binder_->getSlotDurationFromNumerologyIndex(numerologyIndex);
    frame->setDuration(slotDuration);

    // set current position
    lteInfo->setCoord(getRadioPosition());
    lteInfo->setTxPower(txPower_);
    stampExtraTxControlInfo(lteInfo.get());
    frame->setControlInfo(lteInfo.get()->dup());

    EV << "Phy: " << nodeTypeToA(nodeType_) << " with id " << nodeId_
       << " sending message to the air channel. Dest=" << lteInfo->getDestId() << endl;
    transmitFrame(frame, lteInfo.get());
}

const char *PhyBase::airFrameNameFor(const UserControlInfo *info)
{
    switch (info->getFrameType()) {
        case HARQPKT: return "harqFeedback";
        case GRANTPKT: return "harqFeedback-grant";
        case RACPKT: return "rac";
        default: return "airframe";
    }
}

void PhyBase::transmitFrame(LteAirFrame *frame, const UserControlInfo *info)
{
    sendUnicast(frame);
}

void PhyBase::initializeRadio()
{
    primaryRadio_.reference(this, "radioModule", true);
    primaryRadio_->setPhy(this);

    // One radio endpoint per PHY leg, serving every carrier named in its
    // own componentCarrierModules -- iterate that list, in its declaration
    // order. Declaration order is what the binder_->registerCarrierUe
    // sweep below preserves.
    for (auto *cc : primaryRadio_->getComponentCarriers()) {
        GHz carrierFreq = cc->getCarrierFrequency();
        servedCarriers_.insert(carrierFreq);
        if (nodeType_ == UE)
            binder_->registerCarrierUe(carrierFreq, cc->getNumerologyIndex(), nodeId_);
    }
}

void PhyBase::updateDisplayString()
{
    char buf[80] = "";
    if (numAirFrameReceived_ > 0)
        sprintf(buf + strlen(buf), "af_ok:%d ", numAirFrameReceived_);
    if (numAirFrameNotReceived_ > 0)
        sprintf(buf + strlen(buf), "af_no:%d ", numAirFrameNotReceived_);
    getDisplayString().setTagArg("t", 0, buf);
}

void PhyBase::sendBroadcast(LteAirFrame *airFrame)
{
    // Remove control info to allow parsim packing
    if (airFrame->getControlInfo() != nullptr) {
        UserControlInfo *userControlInfo = check_and_cast<UserControlInfo *>(airFrame->removeControlInfo());
        airFrame->setAdditionalInfo(*userControlInfo);
        delete userControlInfo;
    }

    // delegate the ChannelControl to send the airframe
    sendToChannel(airFrame);
}

void PhyBase::sendUnicast(LteAirFrame *frame)
{
    UserControlInfo *ci = check_and_cast<UserControlInfo *>(
            frame->getControlInfo());
    // dest MacNodeId from control info
    MacNodeId dest = ci->getDestId();
    cModule *receiver = binder_->getNodeModule(dest);
    if (receiver == nullptr) {
        // destination node has left the simulation
        delete frame;
        return;
    }

    // Remove control info to allow parsim packing
    if (frame->getControlInfo() != nullptr) {
        UserControlInfo *userControlInfo = check_and_cast<UserControlInfo *>(frame->removeControlInfo());
        frame->setAdditionalInfo(*userControlInfo);
        delete userControlInfo;
    }

    sendDirect(frame, 0, frame->getDuration(), receiver, getReceiverGateIndex(receiver, dest));
}

int PhyBase::getReceiverGateIndex(const cModule *receiver, MacNodeId dest) const
{
    int gate = isNrUe(dest) ? receiver->findGate("nrRadioIn") : receiver->findGate("radioIn");
    if (gate < 0) {
        gate = receiver->findGate("lteRadioIn");
        if (gate < 0) {
            throw cRuntimeError("receiver \"%s\" has no suitable radio input gate",
                    receiver->getFullPath().c_str());
        }
    }
    return gate;
}

} //namespace
