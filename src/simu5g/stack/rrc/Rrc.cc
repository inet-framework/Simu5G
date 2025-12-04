//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/rrc/Rrc.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/rlc/um/LteRlcUm.h"
#include "simu5g/stack/rlc/am/LteRlcAm.h"
#include "simu5g/stack/pdcp/LtePdcp.h"

namespace simu5g {

Define_Module(Rrc);

void Rrc::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        pdcpModule.reference(this, "pdcpModule", true);
        rlcUmModule.reference(this, "rlcUmModule", true);
        nrRlcUmModule.reference(this, "nrRlcUmModule", false);
        macModule.reference(this, "macModule", true);
        nrMacModule.reference(this, "nrMacModule", false);
    }
    else if (stage == INITSTAGE_SIMU5G_NETWORK_CONFIGURATION) {
        cModule *containingNode = getContainingNode(this);
        MacNodeId nodeId = MacNodeId(containingNode->par("macNodeId").intValue());
        nodeType = getNodeTypeById(nodeId);
        if (nodeType == UE) {
            lteNodeId = nodeId;
            if (containingNode->hasPar("nrMacNodeId"))
                nrNodeId = MacNodeId(containingNode->par("nrMacNodeId").intValue());
        }
        if (nodeType == NODEB) {
            bool isNr = containingNode->par("nodeType").stdstringValue() == "GNODEB";
            (isNr ? nrNodeId : lteNodeId) = nodeId;
        }
    }
}

void Rrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void Rrc::createIncomingConnection(FlowControlInfo *lteInfo, bool withPdcp)
{
    Enter_Method_Silent("createIncomingConnection()");

    EV << "Rrc::createIncomingConnection - " << " srcId=" << lteInfo->getSourceId() << " destId=" << lteInfo->getDestId()
        << " groupId=" << lteInfo->getMulticastGroupId() << " lcid=" << lteInfo->getLcid()
        << " direction=" << dirToA((Direction)lteInfo->getDirection())
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(lteInfo->getDestId() == getLteNodeId() || lteInfo->getDestId() == getNrNodeId() || lteInfo->getMulticastGroupId() != NODEID_NONE);

    // Create MAC incoming connection
    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
    MacNodeId senderId = desc.getSourceId();
    LogicalCid lcid = desc.getLcid();
    MacCid cid = MacCid(senderId, lcid);
    auto mac = (nodeType==UE && isNrUe(lteInfo->getDestId())) ? nrMacModule.get() : macModule.get(); //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    mac->createIncomingConnection(cid, desc);

    // RLC: only UM works
    //MacCid cid = ctrlInfoToMacCid(lteInfo);
    MacNodeId nodeIdForCid = (lteInfo->getDirection() == DL) ? lteInfo->getDestId() : lteInfo->getSourceId();
    MacCid cid2 = MacCid(nodeIdForCid, lteInfo->getLcid());
    auto rlcUm = (nodeType==UE && isNrUe(lteInfo->getDestId())) ? nrRlcUmModule.get() : rlcUmModule.get(); //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    rlcUm->createRxBuffer(cid2, lteInfo);

    // PDCP is not needed on Secondary nodes
    if (withPdcp) {
        MacCid cid3 = MacCid(lteInfo->getSourceId(), lteInfo->getLcid());
        pdcpModule->createRxEntity(cid3);
    }
}

void Rrc::createOutgoingConnection(FlowControlInfo *lteInfo, bool withPdcp)
{
    Enter_Method_Silent("createOutgoingConnection()");

    EV << "Rrc::createOutgoingConnection - " << " srcId=" << lteInfo->getSourceId() << " destId=" << lteInfo->getDestId()
        << " groupId=" << lteInfo->getMulticastGroupId() << " lcid=" << lteInfo->getLcid()
        << " direction=" << dirToA((Direction)lteInfo->getDirection())
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(lteInfo->getSourceId() == getLteNodeId() || lteInfo->getSourceId() == getNrNodeId());

    // Create MAC outgoing connection
    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
    MacNodeId destId = desc.getDestId();
    LogicalCid lcid = desc.getLcid();
    MacCid cid = MacCid(destId, lcid);
    auto mac = (nodeType==UE && isNrUe(lteInfo->getSourceId())) ? nrMacModule.get() : macModule.get();
    mac->createOutgoingConnection(cid, desc);

    // RLC: only UM works
    MacCid cid2 = ctrlInfoToMacCid(lteInfo);
    auto rlcUm = (nodeType==UE && isNrUe(lteInfo->getSourceId())) ? nrRlcUmModule.get() : rlcUmModule.get();
    rlcUm->createTxBuffer(cid2, lteInfo);

    // PDCP is not needed on Secondary nodes
    if (withPdcp) {
        MacCid cid3 = MacCid(lteInfo->getDestId(), lteInfo->getLcid());
        pdcpModule->createTxEntity(cid3);
    }
}

} // namespace simu5g
