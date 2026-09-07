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

DcPdcpLegSplitter::~DcPdcpLegSplitter()
{
    delete legSelection_;
}

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
        if (legSelection_ == nullptr)   // unless a bearer definition already pushed one (setLegSelection())
            legSelection_ = getExpressionFromPar(par("legSelection"), new PolicyResolver(this));
    }
}

void DcPdcpLegSplitter::setServingNodeIds(MacNodeId servingNodeId, MacNodeId nrServingNodeId)
{
    Enter_Method_Silent("setServingNodeIds");
    servingNodeId_ = servingNodeId;
    nrServingNodeId_ = nrServingNodeId;
}

void DcPdcpLegSplitter::setLegSelection(const char *spec)
{
    Enter_Method("setLegSelection()");
    std::string src = spec;
    if (src.rfind("expr(", 0) != 0 || src.empty() || src.back() != ')')
        throw cRuntimeError("setLegSelection: '%s' is not an expression written as \"expr(...)\"", spec);
    auto *expr = new cDynamicExpression();
    try {
        expr->parse(src.substr(5, src.size() - 6).c_str());
    }
    catch (std::exception& e) {
        delete expr;
        throw;
    }
    expr->setResolver(new PolicyResolver(this));
    delete legSelection_;
    legSelection_ = expr;
}

bool DcPdcpLegSplitter::isLegLive(int leg, const FlowControlInfo *lteInfo)
{
    if (leg >= numLegs_)
        return false;   // not a leg of this bearer

    // Which technology serves this leg: leg 0 is the anchor stack, whose technology the
    // flow's own (anchor) ids reveal; leg 1 is the other one.
    bool anchorNr = isNrUe(isUe_ ? lteInfo->getSourceId() : lteInfo->getDestId());
    bool legNr = (leg == 0) ? anchorNr : !anchorNr;

    if (isUe_) {
        // this UE's own attachment on the leg's stack, as RRC pushed it -- current as of
        // handover start, ahead of the Binder (see BearerManagement::pushServingNodeIds())
        return (legNr ? nrServingNodeId_ : servingNodeId_) != NODEID_NONE;
    }

    // a base station: the UE's attachment on the stack this leg serves. The network
    // learns of a UE's handover by signaling, so the Binder is the authority here.
    MacNodeId peerId = binder_->getUeNodeId(lteInfo->getDestId(), legNr);
    return peerId != NODEID_NONE && binder_->getServingNodeOrSelf(peerId) != NODEID_NONE;
}

int DcPdcpLegSplitter::selectLeg(const FlowControlInfo *lteInfo)
{
    int liveLegs = 0, lastLiveLeg = 0;
    for (int leg = 0; leg < numLegs_; leg++)
        if (isLegLive(leg, lteInfo)) {
            liveLegs++;
            lastLiveLeg = leg;
        }

    if (liveLegs == 0) {
        EV_WARN << NOW << " DcPdcpLegSplitter - no leg of this bearer is available; falling back to leg 0" << endl;
        return 0;
    }
    if (liveLegs == 1)
        return lastLiveLeg;   // nothing to decide

    currentTypeOfService_ = lteInfo->getTypeOfService();
    currentPacketOrdinal_ = packetsSteered_++;
    int leg = legSelection_->intValue();
    if (leg < 0 || leg >= numLegs_ || !isLegLive(leg, lteInfo))
        throw cRuntimeError("legSelection chose leg %d, which is not a live leg of this bearer", leg);
    return leg;
}

cValue DcPdcpLegSplitter::PolicyResolver::readVariable(cExpression::Context *context, const char *name)
{
    if (!strcmp(name, "typeOfService")) return (intval_t)module_->currentTypeOfService_;
    if (!strcmp(name, "packetOrdinal")) return (intval_t)module_->currentPacketOrdinal_;
    throw cRuntimeError("DcPdcpLegSplitter: unknown variable '%s' in the legSelection expression", name);
}

void DcPdcpLegSplitter::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    int leg = selectLeg(lteInfo.get());

    // Per-leg id adaptation + leg-flavored statistics (moved from NrPdcpTxEntity::deliverPdcpPdu).
    // Leg 0 is the anchor (master cell group) leg; leg 1 is the UE's local secondary stack,
    // or a DC master's remote leg via X2. The flow's tags carry the anchor stack's ids, so
    // the anchor's technology can be read off them (isNrUe), and the secondary stack is the
    // other one -- under EN-DC the anchor is LTE and the secondary NR, under NE-DC reversed.
    if (leg == 0) {
        // anchor leg: ids already correct
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to the anchor leg's RLC" << endl;
        if (hasListeners(pdcpSduSentSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else if (isUe_) {
        // UE's local secondary stack: translate to that stack's ids for its RLC (the
        // serving node is looked up per packet, so handover is honored)
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to the secondary leg's RLC" << endl;
        MacNodeId scgNodeId = isNrUe(lteInfo->getSourceId()) ? nodeId_ : nrNodeId_;
        ASSERT(scgNodeId != NODEID_NONE);
        lteInfo->setSourceId(scgNodeId);
        lteInfo->setDestId(binder_->getServingNodeOrSelf(scgNodeId));
        if (hasListeners(pdcpSduSentNrSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D)
            emit(pdcpSduSentNrSignal_, pkt);
        emit(sentPacketToLowerLayerSignal_, pkt);
    }
    else {
        // DC master's remote leg: rewrite to (secondary node, the UE's secondary-stack id)
        // and address the X2 tunnel; the PDU leaves via the DcMux
        EV << NOW << " DcPdcpLegSplitter - DRB ID[" << lteInfo->getDrbId() << "] - the destination is under the control of a secondary node" << endl;
        MacNodeId secondaryNodeId = binder_->getSecondaryNode(nodeId_);
        ASSERT(secondaryNodeId != NODEID_NONE);
        ASSERT(secondaryNodeId != nodeId_);
        MacNodeId scgDestId = binder_->getUeNodeId(lteInfo->getDestId(), !isNrUe(lteInfo->getDestId()));
        ASSERT(scgDestId != NODEID_NONE);
        lteInfo->setSourceId(secondaryNodeId);
        lteInfo->setDestId(scgDestId);
        pkt->addTagIfAbsent<X2TargetReq>()->setTargetNode(secondaryNodeId);
    }

    send(pkt, "out", leg);
}

} // namespace simu5g
