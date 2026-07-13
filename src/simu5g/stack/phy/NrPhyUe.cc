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

#include "simu5g/stack/phy/NrPhyUe.h"

#include "simu5g/stack/ip2nic/HandoverPacketHolderUe.h"
#include "simu5g/stack/rrc/HandoverController.h"
#include "simu5g/stack/phy/feedback/LteDlFeedbackGenerator.h"
#include "simu5g/stack/rrc/D2dModeSelectionBase.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

Define_Module(NrPhyUe);

// TODO: ***reorganize*** method
void NrPhyUe::handleAirFrame(cMessage *msg)
{
    LteAirFrame *frame = static_cast<LteAirFrame *>(msg);
    UserControlInfo *lteInfo = new UserControlInfo(frame->getAdditionalInfo());

    EV << "NrPhyUe: received new LteAirFrame with ID " << frame->getId() << " from channel" << endl;

    MacNodeId sourceId = lteInfo->getSourceId();
    if (!binder_->nodeExists(sourceId)) {
        // The source has left the simulation
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

    //Update coordinates of this user
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
        // the sender. Hence the filter is scoped to beacons, same as in LtePhyUeD2D.
        if (lteInfo->isNr() != isNr_) {
            EV << "Received beacon packet [from NR=" << lteInfo->isNr() << "] from a different radio technology [to NR=" << isNr_ << "]. Delete it." << endl;
            delete lteInfo;
            delete frame;
            return;
        }

        handoverController_->beaconReceived(frame, lteInfo);
        return;
    }

    // Check if the frame is for us ( MacNodeId matches or - if this is a multicast communication - enrolled in multicast group)
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
     * This could happen if the UE associates with a new master while a packet from the
     * old master is in-flight: the packet is in the air
     * while the UE changes master.
     * Event timing:      TTI x: packet scheduled and sent by the UE (tx time = 1ms)
     *                     TTI x+0.1: UE changes master
     *                     TTI x+1: packet from UE arrives at the old master
     */
    if (lteInfo->getSourceId() != servingNodeId_) {
        EV << "WARNING: Frame from a UE that is leaving this cell (handover): deleted " << endl;
        EV << "Source MacNodeId: " << lteInfo->getSourceId() << endl;
        EV << "UE MacNodeId: " << nodeId_ << endl;
        delete lteInfo;
        delete frame;
        return;
    }

    // Send H-ARQ feedback up
    if (lteInfo->getFrameType() == HARQPKT || lteInfo->getFrameType() == GRANTPKT || lteInfo->getFrameType() == RACPKT) {
        handleControlMsg(frame, lteInfo);
        return;
    }

    // This is a DATA packet

    if (servingNodeId_ == NODEID_NONE) {
        // UE is not (anymore) associated with any eNB/gNB and all harqBuffers are already deleted.
        // Handing this data packet to the MAC layer will lead to null pointers.
        // (Mirrors the same guard in LtePhyUeD2D::handleAirFrame; matters for D2D/D2D_MULTI DATA
        // in-flight during the mid-handover detachment window.)
        EV << "NrPhyUe: UE " << nodeId_ << " received data packet while not associated with any base station. Drop it." << endl;
        delete lteInfo;
        delete frame;
        return;
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

    bool result = channelModel->isReceptionSuccessful(frame, lteInfo);

    // Update statistics
    if (result)
        numAirFrameReceived_++;
    else
        numAirFrameNotReceived_++;

    EV << "Handled LteAirframe with ID " << frame->getId() << " with result "
       << (result ? "RECEIVED" : "NOT RECEIVED") << endl;

    auto pkt = check_and_cast<inet::Packet *>(frame->decapsulate());

    // Here frame has to be destroyed since it is no more useful
    delete frame;

    // Attach the decider result to the packet as control info
    *(pkt->addTagIfAbsent<UserControlInfo>()) = *lteInfo;
    delete lteInfo;

    pkt->addTagIfAbsent<PhyReceptionInd>()->setDeciderResult(result);

    // Send decapsulated message along with result control info to upperGateOut_
    send(pkt, upperGateOut_);

    if (getEnvir()->isGUI())
        updateDisplayString();
}

} //namespace
