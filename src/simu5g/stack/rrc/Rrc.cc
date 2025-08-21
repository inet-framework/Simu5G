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
#include "simu5g/stack/pdcp/LtePdcp.h"

namespace simu5g {

Define_Module(Rrc);

void Rrc::initialize()
{
    // Initialize module references
    macModule.reference(this, "macModule", true);
    rlcModule.reference(this, "rlcModule", true);
    pdcpModule.reference(this, "pdcpModule", true);

    EV << "RRC module initialized with references to MAC, RLC, and PDCP modules" << endl;
}

void Rrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void Rrc::createDataConnection(MacNodeId sourceId, MacNodeId destId,
                             LogicalCid lcid, Direction direction,
                             LteTrafficClass trafficClass, LteRlcType rlcType,
                             const inet::Ipv4Address& srcAddr,
                             const inet::Ipv4Address& dstAddr,
                             uint16_t typeOfService)
{
    Enter_Method_Silent("createDataConnection()");

    // Create FlowDescriptor for data bearer
    FlowDescriptor desc;
    desc.setSourceId(sourceId);
    desc.setDestId(destId);
    desc.setLcid(lcid);
    desc.setDirection(direction);
    desc.setTraffic(trafficClass);
    desc.setRlcType(rlcType);

    // Get MAC module and call createConnection
    macModule->createIncomingConnection(desc);
    macModule->createOutgoingConnection(desc);

    // Get RLC module and call createLogicalChannel
    auto rlcCompoundModule = check_and_cast<cModule*>(rlcModule.get());

    // Determine which RLC submodule to use based on rlcType
    cModule* rlcSubmodule = nullptr;
    switch(rlcType) {
        case TM: rlcSubmodule = rlcCompoundModule->getSubmodule("tm"); break;
        case UM: rlcSubmodule = rlcCompoundModule->getSubmodule("um"); break;
        case AM: rlcSubmodule = rlcCompoundModule->getSubmodule("am"); break;
        default: throw cRuntimeError("RRC::createDataConnection(): Unknown RLC type %d", rlcType);
    }

    if (rlcSubmodule == nullptr)
        throw cRuntimeError("RRC::createDataConnection(): RLC submodule for type %d not found", rlcType);

    MacCid cid(destId, lcid);

    // For now, only implement UM mode
    if (rlcType == UM) {
        auto rlcUm = check_and_cast<LteRlcUm*>(rlcSubmodule);
        rlcUm->createTxBuffer(cid, lteInfo);
        rlcUm->createRxBuffer(cid, lteInfo);
    } else {
        throw cRuntimeError("RRC::createDataConnection(): RLC type %d not yet implemented for createLogicalChannel", rlcType);
    }

    // Get PDCP module and call createLogicalChannel
    pdcpModule->createTxEntity(cid);
    pdcpModule->createRxEntity(cid);

    EV << "RRC::createDataConnection(): Established DRB from " << sourceId
       << " to " << destId << " on LCID " << lcid << " with direction " << direction << endl;
}

} // namespace simu5g
