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

void Rrc::initialize()
{
    macModule.reference(this, "macModule", true);
    pdcpModule.reference(this, "pdcpModule", true);
    rlcUmModule.reference(this, "rlcUmModule", true);
}

void Rrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void Rrc::createIncomingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createIncomingConnection()");

//    // Create MAC incoming connection
//    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
//    macModule->ensureIncomingConnection(desc);

//    // RLC: only UM works
//    MacCid cid = ctrlInfoToMacCid(lteInfo);
//    rlcUmModule->createRxBuffer(cid, lteInfo);

    // PDCP RX entity
    MacCid cid2 = MacCid(lteInfo->getSourceId(), lteInfo->getLcid());
    pdcpModule->createRxEntity(cid2);
}

void Rrc::createOutgoingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createOutgoingConnection()");

    // Create MAC incoming connection
//    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
//    macModule->ensureOutgoingConnection(desc);

//    // RLC: only UM works
//    MacCid cid = ctrlInfoToMacCid(lteInfo);
//    rlcUmModule->createTxBuffer(cid, lteInfo);

    // PDCP TX entity
    MacCid cid2 = MacCid(lteInfo->getDestId(), lteInfo->getLcid());
    pdcpModule->createTxEntity(cid2);
}

} // namespace simu5g
