//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <inet/networklayer/common/NetworkInterface.h>

#include "simu5g/stack/pdcp/NrPdcpUe.h"
#include "simu5g/stack/pdcp/NrTxPdcpEntity.h"
#include "simu5g/stack/pdcp/NrRxPdcpEntity.h"
#include "simu5g/stack/packetFlowManager/PacketFlowManagerBase.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

Define_Module(NrPdcpUe);

void NrPdcpUe::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        inet::NetworkInterface *nic = inet::getContainingNicModule(this);
        dualConnectivityEnabled_ = nic->par("dualConnectivityEnabled").boolValue();

        // initialize gates
        // nrTmSapInGate_ = gate("TM_Sap$i", 1);
        nrTmSapOutGate_ = gate("TM_Sap$o", 1);
        //nrUmSapInGate_ = gate("UM_Sap$i", 1);
        nrUmSapOutGate_ = gate("UM_Sap$o", 1);
        //nrAmSapInGate_ = gate("AM_Sap$i", 1);
        nrAmSapOutGate_ = gate("AM_Sap$o", 1);
    }

    if (stage == inet::INITSTAGE_NETWORK_CONFIGURATION)
        nrNodeId_ = MacNodeId(getContainingNode(this)->par("nrMacNodeId").intValue());

    LtePdcpUeD2D::initialize(stage);
}

MacNodeId NrPdcpUe::getDestId(const Ipv4Address& destAddr, bool useNR, MacNodeId sourceId)
{
    MacNodeId destId = binder_->getMacNodeId(destAddr);
    MacNodeId srcId = useNR ? nrNodeId_ : nodeId_;

    // check whether the destination is inside or outside the LTE network
    if (destId == NODEID_NONE || getDirection(srcId, destId) == UL) {
        // if not, the packet is destined to the eNB

        // UE is subject to handovers: master may change
        return binder_->getNextHop(sourceId);
    }

    return destId;
}

/*
 * Upper Layer handlers
 */

MacCid NrPdcpUe::analyzePacket(inet::Packet *pkt)
{
    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    setTrafficInformation(pkt, lteInfo);

    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();

    // select the correct nodeId for the source
    MacNodeId nodeId = useNR ? nrNodeId_ : nodeId_;
    lteInfo->setSourceId(nodeId);

    // Get IP flow information from the new tag
    auto ipFlowInd = pkt->getTag<IpFlowInd>();
    Ipv4Address srcAddr = ipFlowInd->getSrcAddr();
    Ipv4Address destAddr = ipFlowInd->getDstAddr();
    uint16_t typeOfService = ipFlowInd->getTypeOfService();

    // the direction of the incoming connection is a D2D_MULTI one if the application is of the same type,
    // else the direction will be selected according to the current status of the UE, i.e., D2D or UL
    if (destAddr.isMulticast()) {
        binder_->addD2DMulticastTransmitter(nodeId);

        lteInfo->setDirection(D2D_MULTI);

        // Use dynamic multicast node ID allocation instead of computed group ID
        MacNodeId multicastDestId = binder_->getOrAllocateMulticastDestId(destAddr);
        lteInfo->setDestId(multicastDestId);
        EV << "Allocated node ID " << multicastDestId << " for multicast address " << destAddr << endl;
    }
    else {
        MacNodeId destId = binder_->getMacNodeId(destAddr);
        if (destId != NODEID_NONE) { // the destination is a UE within the LTE network
            if (binder_->checkD2DCapability(nodeId, destId)) {
                // this way, we record the ID of the endpoints even if the connection is currently in IM
                // this is useful for mode switching
                lteInfo->setD2dTxPeerId(nodeId);
                lteInfo->setD2dRxPeerId(destId);
            }
            else {
                lteInfo->setD2dTxPeerId(NODEID_NONE);
                lteInfo->setD2dRxPeerId(NODEID_NONE);
            }

            // set actual flow direction based (D2D/UL) based on the current mode (DM/IM) of this pairing
            lteInfo->setDirection(getDirection(nodeId, destId));
        }
        else { // the destination is outside the LTE network
            lteInfo->setDirection(UL);
            lteInfo->setD2dTxPeerId(NODEID_NONE);
            lteInfo->setD2dRxPeerId(NODEID_NONE);
        }
    }

    // Cid Request
    EV << "NrPdcpUe : Received CID request for Traffic [ " << "Source: " << srcAddr
       << " Destination: " << destAddr
       << " , ToS: " << typeOfService
       << " , Direction: " << dirToA((Direction)lteInfo->getDirection()) << " ]\n";

    /*
     * Different LCIDs for different directions of the same flow are assigned.
     * The RLC layer will create different RLC entities for different LCIDs
     */

    ConnectionKey key{srcAddr, destAddr, typeOfService, lteInfo->getDirection()};
    LogicalCid lcid = lookupOrAssignLcid(key);

    // assign LCID
    lteInfo->setLcid(lcid);

    EV << "NrPdcpUe : Assigned Lcid: " << lcid << "\n";
    EV << "NrPdcpUe : Assigned Node ID: " << nodeId << "\n";

    // get effective next hop dest ID
    MacNodeId destId = getDestId(destAddr, useNR, lteInfo->getSourceId());

    // obtain CID
    return MacCid(destId, lcid);
}

void NrPdcpUe::deleteEntities(MacNodeId nodeId)
{
    // delete connections related to the given master nodeB only
    // (the UE might have dual connectivity enabled)
    for (auto tit = txEntities_.begin(); tit != txEntities_.end(); ) {
        if (tit->first.getNodeId() == nodeId) {
            tit->second->deleteModule();  // Delete Entity
            tit = txEntities_.erase(tit);       // Delete Element
        }
        else {
            ++tit;
        }
    }
    for (auto rit = rxEntities_.begin(); rit != rxEntities_.end(); ) {
        if (rit->first.getNodeId() == nodeId) {
            rit->second->deleteModule();  // Delete Entity
            rit = rxEntities_.erase(rit);       // Delete Element
        }
        else
            ++rit;
    }
}

void NrPdcpUe::sendToLowerLayer(Packet *pkt)
{
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();

    if (!dualConnectivityEnabled_ || useNR) {
        EV << "NrPdcpUe : Sending packet " << pkt->getName() << " on port "
           << (lteInfo->getRlcType() == UM ? "NR_UM_Sap$o\n" : "NR_AM_Sap$o\n");

        // use NR id as source
        lteInfo->setSourceId(nrNodeId_);

        // notify the packetFlowManager only with UL packet
        if (lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D) {
            if (NRpacketFlowManager_ != nullptr) {
                EV << "LteTxPdcpEntity::handlePacketFromUpperLayer - notify NRpacketFlowManager_" << endl;
                NRpacketFlowManager_->insertPdcpSdu(pkt);
            }
        }

        // Send message
        send(pkt, (lteInfo->getRlcType() == UM ? nrUmSapOutGate_ : nrAmSapOutGate_));

        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else
        LtePdcpBase::sendToLowerLayer(pkt);
}

} //namespace
