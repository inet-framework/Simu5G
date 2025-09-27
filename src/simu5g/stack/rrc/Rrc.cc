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
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

Define_Module(Rrc);

void Rrc::initialize()
{
    macModule.reference(this, "macModule", true);
    pdcpModule.reference(this, "pdcpModule", true);
    rlcUmModule.reference(this, "rlcUmModule", true);

    // Get binder and node ID for dual connectivity support
    binder_ = inet::getModuleFromPar<Binder>(par("binderModule"), this);
    nodeId_ = macModule->getMacNodeId();
}

void Rrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void Rrc::createIncomingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createIncomingConnection()");

    // Always create MAC/RLC on the current node (local)
//    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
//    macModule->ensureIncomingConnection(desc);
//    MacCid cid = ctrlInfoToMacCid(lteInfo);
//    rlcUmModule->createRxBuffer(cid, lteInfo);

    // For PDCP: check if this is a secondary node in dual connectivity
    MacNodeId masterId = binder_->getMasterNode(nodeId_);
    if ((getNodeTypeById(nodeId_) == ENODEB || getNodeTypeById(nodeId_) == GNODEB) && nodeId_ != masterId) {
        // This is a secondary node - create PDCP on master node
        EV << "Rrc::createIncomingConnection - Secondary node " << nodeId_
           << " creating PDCP RX entity on master node " << masterId << endl;

        Rrc *masterRrc = check_and_cast<Rrc*>(binder_->getMacFromMacNodeId(masterId)->getModuleByPath("^.rrc"));
        MacCid cid2 = MacCid(lteInfo->getSourceId(), lteInfo->getLcid());

        // Check if RX entity already exists to avoid duplicates
        if (masterRrc->pdcpModule->lookupRxEntity(cid2) == nullptr) {
            masterRrc->pdcpModule->createRxEntity(cid2);
        }
    } else {
        // This is a master node or regular node - create PDCP locally
        MacCid cid2 = MacCid(lteInfo->getSourceId(), lteInfo->getLcid());

        // Check if RX entity already exists to avoid duplicates
        if (pdcpModule->lookupRxEntity(cid2) == nullptr) {
            pdcpModule->createRxEntity(cid2);
        }
    }
}

void Rrc::createOutgoingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createOutgoingConnection()");

    // Always create MAC/RLC on the current node (local)
//    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
//    macModule->ensureOutgoingConnection(desc);
//    MacCid cid = ctrlInfoToMacCid(lteInfo);
//    rlcUmModule->createTxBuffer(cid, lteInfo);

    // For PDCP: check if this is a secondary node in dual connectivity
    MacNodeId masterId = binder_->getMasterNode(nodeId_);
    if ((getNodeTypeById(nodeId_) == ENODEB || getNodeTypeById(nodeId_) == GNODEB) && nodeId_ != masterId) {
        // This is a secondary node - create PDCP on master node
        EV << "Rrc::createOutgoingConnection - Secondary node " << nodeId_
           << " creating PDCP TX entity on master node " << masterId << endl;

        Rrc *masterRrc = check_and_cast<Rrc*>(binder_->getMacFromMacNodeId(masterId)->getModuleByPath("^.rrc"));
        MacCid cid2 = MacCid(lteInfo->getDestId(), lteInfo->getLcid());

        // Check if TX entity already exists to avoid duplicates
        if (masterRrc->pdcpModule->lookupTxEntity(cid2) == nullptr) {
            masterRrc->pdcpModule->createTxEntity(cid2);
        }
    } else {
        // This is a master node or regular node - create PDCP locally
        MacCid cid2 = MacCid(lteInfo->getDestId(), lteInfo->getLcid());

        // Check if TX entity already exists to avoid duplicates
        if (pdcpModule->lookupTxEntity(cid2) == nullptr) {
            pdcpModule->createTxEntity(cid2);
        }
    }
}

} // namespace simu5g
