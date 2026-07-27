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

#include "simu5g/stack/pdcp/PdcpLegSplitter.h"

#include <inet/common/packet/Packet.h>
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(PdcpLegSplitter);

simsignal_t PdcpLegSplitter::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");
simsignal_t PdcpLegSplitter::pdcpSduSentSignal_ = registerSignal("pdcpSduSent");
simsignal_t PdcpLegSplitter::pdcpSduSentNrSignal_ = registerSignal("pdcpSduSentNr");

void PdcpLegSplitter::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);

        numLegs_ = par("numLegs");
        auto *legs = check_and_cast<cValueArray *>(par("legs").objectValue());
        if (legs->size() != numLegs_)
            throw cRuntimeError("PdcpLegSplitter: legs descriptor has %d entries, expected numLegs=%d", legs->size(), numLegs_);
        for (int i = 0; i < numLegs_; i++)
            legRats_.push_back(check_and_cast<cValueMap *>(legs->get(i).objectValue())->get("rat").stdstringValue());

        cModule *node = inet::getContainingNode(this);
        nodeId_ = MacNodeId(node->par("macNodeId").intValue());
        if (node->hasPar("nrMacNodeId"))
            nrNodeId_ = MacNodeId(node->par("nrMacNodeId").intValue());
    }
}

void PdcpLegSplitter::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    // Leg choice: follow the TechnologyReq steering tag (see NED comment; the
    // legSelectionRule policy hook replaces this in a later phase)
    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();
    int leg = useNR ? 1 : 0;
    if (leg >= numLegs_ || !gate("out", leg)->isConnected()) {
        EV_WARN << NOW << " PdcpLegSplitter - leg " << leg << " is not available (torn down?); falling back to leg 0" << endl;
        leg = 0;
    }

    // Per-leg id adaptation + leg-flavored statistics (moved from NrTxPdcpEntity::deliverPdcpPdu)
    const std::string& rat = legRats_[leg];
    if (rat == "nr") {
        // UE's local NR stack: translate to the NR-leg ids for the NR RLC (the serving node
        // is looked up per packet, so handover is honored)
        EV << NOW << " PdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to NR RLC" << endl;
        lteInfo->setSourceId(nrNodeId_);
        lteInfo->setDestId(binder_->getServingNodeOrSelf(nrNodeId_));
        if (hasListeners(pdcpSduSentNrSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentNrSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else if (rat == "x2") {
        // DC master's remote leg: rewrite to the secondary gNB / NR UE ids and address the
        // X2 tunnel; the PDU leaves via the DcMux
        EV << NOW << " PdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - the destination is under the control of a secondary node" << endl;
        MacNodeId secondaryNodeId = binder_->getSecondaryNode(nodeId_);
        ASSERT(secondaryNodeId != NODEID_NONE);
        ASSERT(secondaryNodeId != nodeId_);
        MacNodeId nrDestId = binder_->getUeNodeId(lteInfo->getDestId(), true);
        ASSERT(nrDestId != NODEID_NONE);
        lteInfo->setSourceId(secondaryNodeId);
        lteInfo->setDestId(nrDestId);
        pkt->addTagIfAbsent<X2TargetReq>()->setTargetNode(secondaryNodeId);
    }
    else { // "lte": local leg, ids already correct
        EV << NOW << " PdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to LTE RLC" << endl;
        if (hasListeners(pdcpSduSentSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }

    send(pkt, "out", leg);
}

} // namespace simu5g
