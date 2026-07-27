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

#include "simu5g/stack/pdcp/DcMux.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/x2/packet/X2ControlInfo_m.h"
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/NetworkInterface.h>

namespace simu5g {

Define_Module(DcMux);

void DcMux::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
        nodeId_ = MacNodeId(inet::getContainingNode(this)->par("macNodeId").intValue());

        bearerManagement_ = inet::getModuleFromPar<BearerManagement>(par("bearerManagementModule"), this);

        dcManagerInGate_ = gate("dcManagerIn");
    }
}

void DcMux::handleMessage(cMessage *msg)
{
    cGate *incoming = msg->getArrivalGate();
    if (incoming == dcManagerInGate_) {
        // Incoming from DC manager via X2
        auto pkt = check_and_cast<inet::Packet *>(msg);
        auto tag = pkt->removeTag<X2SourceNodeInd>();
        MacNodeId sourceNode = tag->getSourceNode();

        auto ctrlInfo = pkt->getTag<FlowControlInfo>();
        if (ctrlInfo->getDirection() == DL) {
            // DL: master sent data for a UE — dispatch to this bearer's X2 relay
            MacNodeId destId = ctrlInfo->getDestId();
            DrbKey id = DrbKey(destId, ctrlInfo->getDrbId());
            cModule *relay = bearerManagement_->lookupPdcpRelayEntityModule(id);
            ASSERT(relay != nullptr);

            EV << NOW << " DcMux::handleMessage - Received DL PDCP PDU from master node " << sourceNode
               << " for UE " << destId << " - dispatching to the X2 relay entity" << endl;
            // path start = our toRelayEntity gate (crosses the compound boundary into its DL relay)
            send(pkt, relay->gate("x2In")->getPathStartGate());
        }
        else {
            // UL: secondary forwarded UL data — dispatch directly to RX entity
            auto lteInfo = pkt->getTag<FlowControlInfo>();
            // The sourceId may be the NR UE ID; translate to LTE UE ID for RX entity lookup
            MacNodeId sourceId = lteInfo->getSourceId();
            if (isNrUe(sourceId))
                sourceId = binder_->getUeNodeId(sourceId, false);
            DrbKey id = DrbKey(sourceId, lteInfo->getDrbId());
            cModule *pdcpEnt = bearerManagement_->lookupPdcpEntityModule(id);
            ASSERT(pdcpEnt != nullptr);

            EV << NOW << " DcMux::handleMessage - Received UL PDCP PDU from secondary node " << sourceNode
               << " for " << id << " - dispatching to the PDCP entity's remote leg" << endl;
            // the remote (X2) leg is leg 1 of the master's PDCP entity compound; path start =
            // our toRxEntity gate (crosses the compound boundary into its joiner)
            send(pkt, pdcpEnt->gate("legIn", 1)->getPathStartGate());
        }
    }
    else if (incoming->isName("fromEntity")) {
        // Outgoing from PDCP entity: forward to DC manager
        send(msg, "dcManagerOut");
    }
    else {
        throw cRuntimeError("DcMux: unexpected message from gate %s", incoming->getFullName());
    }
}

} // namespace simu5g
