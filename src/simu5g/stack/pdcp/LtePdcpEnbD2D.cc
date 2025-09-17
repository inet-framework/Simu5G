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

#include "simu5g/stack/pdcp/LtePdcpEnbD2D.h"

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/d2dModeSelection/D2DModeSwitchNotification_m.h"
#include "simu5g/stack/packetFlowManager/PacketFlowManagerBase.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

Define_Module(LtePdcpEnbD2D);

using namespace omnetpp;
using namespace inet;

/*
 * Upper Layer handlers
 */

MacCid LtePdcpEnbD2D::analyzePacket(inet::Packet *pkt)
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

    MacNodeId srcId, destId;

    // set direction based on the destination Id. If the destination can be reached
    // using D2D, set D2D direction. Otherwise, set UL direction
    srcId = binder_->getMacNodeId(srcAddr);
    destId = binder_->getMacNodeId(destAddr);   // get final destination

    // check if src and dest of the flow are D2D-capable (currently in IM)
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

    // Cid Request
    EV << "LtePdcpEnbD2D : Received CID request for Traffic [ " << "Source: " << srcAddr
       << " Destination: " << destAddr
       << " , ToS: " << typeOfService
       << " , Direction: " << dirToA((Direction)lteInfo->getDirection()) << " ]\n";

    /*
     * Different LCID for different directions of the same flow are assigned.
     * RLC layer will create different RLC entities for different LCIDs
     */

    ConnectionKey key{srcAddr, destAddr, typeOfService, lteInfo->getDirection()};
    LogicalCid lcid = lookupOrAssignLcid(key);

    // assign LCID
    lteInfo->setLcid(lcid);
    lteInfo->setSourceId(nodeId_);

    // get effective next hop dest ID
    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();
    destId = getNextHopNodeId(destAddr, useNR, lteInfo->getSourceId());  //TODO this value is NOT set on LteInfo -- is this correct?

    // this is the body of former LteTxPdcpEntity::setIds()
    lteInfo->setSourceId(getNodeId());   // TODO CHANGE HERE!!! Must be the NR node ID if this is an NR connection
    if (lteInfo->getMulticastGroupId() != NODEID_NONE)                                               // destId is meaningless for multicast D2D (we use the id of the source for statistic purposes at lower levels)
        lteInfo->setDestId(getNodeId());
    else {
        Ipv4Address destAddr = pkt->getTag<IpFlowInd>()->getDstAddr();
        lteInfo->setDestId(getNextHopNodeId(destAddr, false, lteInfo->getSourceId()));
    }

    // obtain CID
    return MacCid(destId, lcid);
}

void LtePdcpEnbD2D::initialize(int stage)
{
    LtePdcpEnb::initialize(stage);
}

void LtePdcpEnbD2D::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto chunk = pkt->peekAtFront<Chunk>();

    // check whether the message is a notification for mode switch
    if (inet::dynamicPtrCast<const D2DModeSwitchNotification>(chunk) != nullptr) {
        EV << "LtePdcpEnbD2D::handleMessage - Received packet " << pkt->getName() << " from port " << pkt->getArrivalGate()->getName() << endl;

        auto switchPkt = pkt->peekAtFront<D2DModeSwitchNotification>();
        // call handler
        pdcpHandleD2DModeSwitch(switchPkt->getPeerId(), switchPkt->getNewMode());
        delete pkt;
    }
    else {
        LtePdcpEnb::handleMessage(msg);
    }
}

void LtePdcpEnbD2D::pdcpHandleD2DModeSwitch(MacNodeId peerId, LteD2DMode newMode)
{
    EV << NOW << " LtePdcpEnbD2D::pdcpHandleD2DModeSwitch - peering with UE " << peerId << " set to " << d2dModeToA(newMode) << endl;

    // add here specific behavior for handling mode switch at the PDCP layer
}

} //namespace
