#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include "simu5g/stack/ip2nic/LegSelectionBase.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace inet;

void LegSelectionBase::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        lowerLayerOut_ = gate("lowerLayerOut");
        nodeType_ = aToNodeType(par("nodeType").stdstringValue());
    }
}

void LegSelectionBase::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);

    auto ipHeader = pkt->peekAtFront<inet::Ipv4Header>();
    auto srcAddr = ipHeader->getSrcAddress();
    auto destAddr = ipHeader->getDestAddress();
    short int tos = ipHeader->getTypeOfService();

    // Set evaluation context
    currentTypeOfService_ = tos;

    int leg = selectLeg(pkt, srcAddr, destAddr, tos);
    if (leg == DROP_PACKET) {
        delete pkt;
        return;
    }

    pkt->addTagIfAbsent<LegReq>()->setLeg(leg);
    send(pkt, lowerLayerOut_);
}

cDynamicExpression *LegSelectionBase::makePolicyExpression(cPar& par)
{
    cObject *obj = par.objectValue();
    auto *exprObj = dynamic_cast<cOwnedDynamicExpression *>(obj);
    if (!exprObj)
        throw cRuntimeError("Parameter '%s' must be an expr() expression", par.getFullPath().c_str());
    auto *expr = exprObj->dup();
    expr->setResolver(new PolicyResolver(this));
    return expr;
}

void LegSelectionBase::computePacketOrdinal(inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService)
{
    FlowKey key{srcAddr.getInt(), destAddr.getInt(), (uint16_t)typeOfService};
    currentPacketOrdinal_ = splitBearersTable_[key]++;
}

cValue LegSelectionBase::PolicyResolver::readVariable(cExpression::Context *context, const char *name)
{
    if (!strcmp(name, "typeOfService")) return (intval_t)module_->currentTypeOfService_;
    if (!strcmp(name, "packetOrdinal")) return (intval_t)module_->currentPacketOrdinal_;
    throw cRuntimeError("%s: unknown variable '%s' in policy expression", module_->getComponentType()->getName(), name);
}

} //namespace
