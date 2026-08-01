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

#include "simu5g/stack/pdcp/DcPdcpLegSplitter.h"

#include <inet/common/packet/Packet.h>
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(DcPdcpLegSplitter);

simsignal_t DcPdcpLegSplitter::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");
simsignal_t DcPdcpLegSplitter::pdcpSduSentSignal_ = registerSignal("pdcpSduSent");
simsignal_t DcPdcpLegSplitter::pdcpSduSentNrSignal_ = registerSignal("pdcpSduSentNr");

void DcPdcpLegSplitter::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);

        numLegs_ = par("numLegs");

        cModule *node = inet::getContainingNode(this);
        nodeId_ = MacNodeId(node->par("macNodeId").intValue());
        if (node->hasPar("nrMacNodeId"))
            nrNodeId_ = MacNodeId(node->par("nrMacNodeId").intValue());
        isUe_ = (getNodeTypeById(nodeId_) == UE);
    }
}

void DcPdcpLegSplitter::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    // Leg choice: follow the LegReq tag -- the per-packet leg decision is owned by
    // LegSelection; this module only executes it.
    int leg = pkt->getTag<LegReq>()->getLeg();
    if (leg >= numLegs_ || !gate("out", leg)->isConnected()) {
        EV_WARN << NOW << " DcPdcpLegSplitter - leg " << leg << " is not available (torn down?); falling back to leg 0" << endl;
        leg = 0;
    }

    // Per-leg id adaptation + leg-flavored statistics (moved from NrPdcpTxEntity::deliverPdcpPdu).
    // Leg 0 is the local LTE leg; leg 1 is the UE's local NR stack, or a DC master's remote
    // leg via X2.
    if (leg == 0) {
        // local LTE leg: ids already correct
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to LTE RLC" << endl;
        if (hasListeners(pdcpSduSentSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else if (isUe_) {
        // UE's local NR stack: translate to the NR-leg ids for the NR RLC (the serving node
        // is looked up per packet, so handover is honored)
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to NR RLC" << endl;
        lteInfo->setSourceId(nrNodeId_);
        lteInfo->setDestId(binder_->getServingNodeOrSelf(nrNodeId_));
        if (hasListeners(pdcpSduSentNrSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentNrSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else {
        // DC master's remote leg: rewrite to the secondary gNB / NR UE ids and address the
        // X2 tunnel; the PDU leaves via the DcMux
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - the destination is under the control of a secondary node" << endl;
        MacNodeId secondaryNodeId = binder_->getSecondaryNode(nodeId_);
        ASSERT(secondaryNodeId != NODEID_NONE);
        ASSERT(secondaryNodeId != nodeId_);
        MacNodeId nrDestId = binder_->getUeNodeId(lteInfo->getDestId(), true);
        ASSERT(nrDestId != NODEID_NONE);
        lteInfo->setSourceId(secondaryNodeId);
        lteInfo->setDestId(nrDestId);
        pkt->addTagIfAbsent<X2TargetReq>()->setTargetNode(secondaryNodeId);
    }

    send(pkt, "out", leg);
}

} // namespace simu5g
