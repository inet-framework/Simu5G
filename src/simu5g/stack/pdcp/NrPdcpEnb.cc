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
#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/stack/pdcp/NrPdcpEnb.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowObserverBase.h"
#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"

namespace simu5g {

Define_Module(NrPdcpEnb);

void NrPdcpEnb::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        inet::NetworkInterface *nic = inet::getContainingNicModule(this);
        dualConnectivityEnabled_ = nic->par("dualConnectivityEnabled").boolValue();
        if (dualConnectivityEnabled_)
            dualConnectivityManager_.reference(this, "dualConnectivityManagerModule", true);
    }
    LtePdcpEnbD2D::initialize(stage);
}

/*
 * Upper Layer handlers
 */

void NrPdcpEnb::analyzePacket(inet::Packet *pkt)
{
    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();

    // Traffic category, RLC type
    LteTrafficClass trafficCategory = getTrafficCategory(pkt);
    LteRlcType rlcType = getRlcType(trafficCategory);
    lteInfo->setTraffic(trafficCategory);
    lteInfo->setRlcType(rlcType);

    // direction of transmitted packets depends on node type
    Direction dir = getNodeTypeById(nodeId_) == UE ? UL : DL;
    lteInfo->setDirection(dir);

    // Get IP flow information from the new tag
    auto ipFlowInd = pkt->getTag<IpFlowInd>();
    Ipv4Address srcAddr = ipFlowInd->getSrcAddr();
    Ipv4Address destAddr = ipFlowInd->getDstAddr();
    uint16_t typeOfService = ipFlowInd->getTypeOfService();

    // Get technology selection from the new tag
    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();

    MacNodeId srcId, destId;

    // set direction based on the destination Id. If the destination can be reached
    // using D2D, set D2D direction. Otherwise, set UL direction
    srcId = useNR ? binder_->getNrMacNodeId(srcAddr) : binder_->getMacNodeId(srcAddr);
    destId = useNR ? binder_->getNrMacNodeId(destAddr) : binder_->getMacNodeId(destAddr);   // get final destination

    // check if src and dest of the flow are D2D-capable UEs (currently in IM)
    if (getNodeTypeById(srcId) == UE && getNodeTypeById(destId) == UE && binder_->getD2DCapability(srcId, destId)) {
        // this way, we record the ID of the endpoint even if the connection is in IM
        // this is useful for mode switching
        lteInfo->setD2dTxPeerId(srcId);
        lteInfo->setD2dRxPeerId(destId);
    }
    else {
        lteInfo->setD2dTxPeerId(NODEID_NONE);
        lteInfo->setD2dRxPeerId(NODEID_NONE);
    }

    // this is the body of former NrTxPdcpEntity::setIds()
    if (useNR && getNodeTypeById(getNodeId()) != ENODEB && getNodeTypeById(getNodeId()) != GNODEB)
        lteInfo->setSourceId(getNrNodeId());
    else
        lteInfo->setSourceId(getNodeId());
    if (lteInfo->getMulticastGroupId() != NODEID_NONE)                                               // destId is meaningless for multicast D2D (we use the id of the source for statistical purposes at lower levels)
        lteInfo->setDestId(getNodeId());
    else {
        Ipv4Address destAddr = pkt->getTag<IpFlowInd>()->getDstAddr();
        lteInfo->setDestId(getNextHopNodeId(destAddr, useNR, lteInfo->getSourceId()));
    }

    // CID Request
    EV << "NrPdcpEnb : Received CID request for Traffic [ " << "Source: " << srcAddr
       << " Destination: " << destAddr
       << " , ToS: " << typeOfService
       << " , Direction: " << dirToA((Direction)lteInfo->getDirection()) << " ]\n";

    /*
     * Different LCID for different directions of the same flow are assigned.
     * RLC layer will create different RLC entities for different LCIDs
     */

    ConnectionKey key{srcAddr, destAddr, typeOfService, lteInfo->getDirection()};
    LogicalCid lcid = lookupOrAssignLcid(key);

    // Dual Connectivity: adjust source and dest IDs for downlink packets in DC scenarios.
    // If this is a master eNB in DC and there's a secondary for this UE which will get this packet via X2 and transmit it via its RAN
    MacNodeId secondaryNodeId = binder_->getSecondaryNode(nodeId_);
    if (dualConnectivityEnabled_ && secondaryNodeId != NODEID_NONE && useNR) {
        lteInfo->setSourceId(secondaryNodeId);
        lteInfo->setDestId(binder_->getNrMacNodeId(destAddr)); // use NR nodeId of the UE
    }

    // assign LCID
    lteInfo->setLcid(lcid);
}

void NrPdcpEnb::fromLowerLayer(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    pkt->trim();

    // if dual connectivity is enabled and this is a secondary node,
    // forward the packet to the PDCP of the master node
    MacNodeId masterId = binder_->getMasterNodeOrSelf(nodeId_);
    if (dualConnectivityEnabled_ && (nodeId_ != masterId)) {
        EV << NOW << " NrPdcpEnb::fromLowerLayer - forward packet to the master node - id [" << masterId << "]" << endl;
        forwardDataToTargetNode(pkt, masterId);
        return;
    }

    LtePdcpEnbD2D::fromLowerLayer(pktAux);
}

MacNodeId NrPdcpEnb::getNextHopNodeId(const Ipv4Address& destAddr, bool useNR, MacNodeId sourceId)
{
    MacNodeId destId;
    if (!dualConnectivityEnabled_ || useNR)
        destId = binder_->getNrMacNodeId(destAddr);
    else
        destId = binder_->getMacNodeId(destAddr);

    // master of this UE
    MacNodeId master = binder_->getNextHop(destId);
    if (master != nodeId_) {
        destId = master;
    }
    else {
        // for dual connectivity
        master = binder_->getMasterNodeOrSelf(master);
        if (master != nodeId_) {
            destId = master;
        }
    }
    // else UE is directly attached
    return destId;
}

void NrPdcpEnb::forwardDataToTargetNode(Packet *pkt, MacNodeId targetNode)
{
    EV << NOW << " NrPdcpEnb::forwardDataToTargetNode - Send PDCP packet to node with id " << targetNode << endl;
    dualConnectivityManager_->forwardDataToTargetNode(pkt, targetNode);
}

void NrPdcpEnb::receiveDataFromSourceNode(Packet *pkt, MacNodeId sourceNode)
{
    Enter_Method("receiveDataFromSourceNode");
    take(pkt);

    auto ctrlInfo = pkt->getTag<FlowControlInfo>();
    if (ctrlInfo->getDirection() == DL) {
        MacNodeId destId = ctrlInfo->getDestId();
        EV << NOW << " NrPdcpEnb::receiveDataFromSourceNode - Received PDCP PDU from master node with id " << sourceNode << " - destination node[" << destId << "]" << endl;
        sendToLowerLayer(pkt);
    }
    else { // UL
        EV << NOW << " NrPdcpEnb::receiveDataFromSourceNode - Received PDCP PDU from secondary node with id " << sourceNode << endl;
        fromLowerLayer(pkt);
    }
}

void NrPdcpEnb::activeUeUL(std::set<MacNodeId> *ueSet)
{
    for (const auto& [cid, rxEntity] : rxEntities_) {
        MacNodeId nodeId = cid.getNodeId();
        if (!(rxEntity->isEmpty()))
            ueSet->insert(nodeId);
    }
}

} //namespace
