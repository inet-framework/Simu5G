//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/ip2nic/SlIp2Nic.h"

#include <inet/common/socket/SocketTag_m.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/common/QfiTag_m.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;

Define_Module(SlIp2Nic);

void SlIp2Nic::initialize(int stage)
{
    Ip2Nic::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        slBinder_ = SlBinder::getInstance();
        slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
        bearerManagement_ = check_and_cast<BearerManagement *>(getModuleByPath(par("bearerManagementModule").stringValue()));
        pc5UnicastEnabled_ = par("pc5UnicastEnabled");
        hasSlSdap_ = par("hasSlSdap");
        useDscpAsPfiFallback_ = par("useDscpAsPfiFallback");

        // Uu/PC5 path-selection policy (D33)
        const char *policyName = par("pathSelectionPolicy");
        if (!SlPathPolicy::parse(policyName, pathPolicy_))
            throw cRuntimeError("SlIp2Nic: unknown pathSelectionPolicy '%s' "
                                "(expected pc5IfPeer/uuIfServed/pc5Only/condition)", policyName);
        if (pathPolicy_ == SlPathPolicy::CONDITION) {
            cObject *obj = par("pc5Condition").objectValue();
            auto *exprObj = dynamic_cast<cOwnedDynamicExpression *>(obj);
            if (exprObj == nullptr)
                throw cRuntimeError("SlIp2Nic: pathSelectionPolicy=\"condition\" needs an expr() in pc5Condition");
            pc5ConditionExpr_ = exprObj->dup();
            pc5ConditionExpr_->setResolver(new PathPolicyResolver(this));
        }
    }
}

SlIp2Nic::~SlIp2Nic()
{
    delete pc5ConditionExpr_;
}

cValue SlIp2Nic::PathPolicyResolver::readVariable(cExpression::Context *context, const char *name)
{
    if (!strcmp(name, "tos")) return (intval_t)module_->pathVars_.tos;
    if (!strcmp(name, "served")) return module_->pathVars_.served;
    if (!strcmp(name, "peerSlCapable")) return module_->pathVars_.peerSlCapable;
    throw cRuntimeError("SlIp2Nic: unknown variable '%s' in the pc5Condition expression "
                        "(available: tos, served, peerSlCapable)", name);
}

SlPathPolicy::Decision SlIp2Nic::decidePath(Ipv4Address destAddr, int tos, MacNodeId *outPeerId)
{
    // groupcast/broadcast destinations have no Uu equivalent: PC5 under
    // every policy (D33)
    if (slBinder_->getDstL2IdForMulticastAddress(destAddr) != SL_L2ID_NONE)
        return SlPathPolicy::PATH_PC5;

    if (!pc5UnicastEnabled_)
        return SlPathPolicy::PATH_UU;

    MacNodeId peerId = slBinder_->getPc5UnicastPeer(binder_.get(), destAddr, nrNodeId_);
    bool peerSlCapable = (peerId != NODEID_NONE);
    bool served = (binder_->getServingNode(nrNodeId_) != NODEID_NONE);

    bool conditionResult = false;
    if (pathPolicy_ == SlPathPolicy::CONDITION && peerSlCapable) {
        pathVars_ = { tos, served, peerSlCapable };
        conditionResult = pc5ConditionExpr_->evaluate().boolValue();
    }

    SlPathPolicy::Decision d = SlPathPolicy::decideUnicast(pathPolicy_, peerSlCapable, served, conditionResult);
    if (d == SlPathPolicy::PATH_PC5 && outPeerId != nullptr)
        *outPeerId = peerId;
    return d;
}

void SlIp2Nic::handleMessage(cMessage *msg)
{
    cGate *arrival = msg->getArrivalGate();
    if (arrival->isName("sdapTxIn")) {
        onSdapTxReturn(check_and_cast<Packet *>(msg));
        return;
    }
    if (arrival->isName("sdapRxIn")) {
        onSdapRxReturn(check_and_cast<Packet *>(msg));
        return;
    }
    if (hasSlSdap_ && arrival->isName("stackIn")) {
        // received SL packets take the RX side chain (D20) with their tags
        // intact; the base tag strip continues on return
        auto pkt = check_and_cast<Packet *>(msg);
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr && lteInfo->getDirection() == SL) {
            routeViaSdapRx(pkt, (uint32_t)num(lteInfo->getSourceId()));
            return;
        }
    }
    Ip2Nic::handleMessage(msg);
}

void SlIp2Nic::toStackUe(Packet *pkt)
{
    EV << "SlIp2Nic::toStackUe - message from IP layer: send to stack: " << pkt->str() << std::endl;
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    packetHeld_ = false;
    analyzePacket(pkt, ipHeader->getSrcAddress(), ipHeader->getDestAddress(), ipHeader->getTypeOfService());
    if (packetHeld_)
        return;  // ownership transferred to SlRrc until the link is up (D23)

    // classified SL packets take the SDAP side chain (D20): the entity maps
    // the resolved PFI to the serving SLRB and returns the packet
    if (hasSlSdap_) {
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr && lteInfo->getDirection() == SL) {
            int pfi = resolvePfi(pkt);
            pkt->addTagIfAbsent<QfiReq>()->setQfi(Qfi(pfi));
            routeViaSdapTx(pkt, slBinder_->getL2IdForNodeId(lteInfo->getDestId()));
            return;
        }
    }

    send(pkt, stackGateOut_);
}

void SlIp2Nic::analyzePacket(Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    SlL2Id dstL2Id = slBinder_->getDstL2IdForMulticastAddress(destAddr);
    if (dstL2Id == SL_L2ID_NONE) {
        // PC5 unicast classification via the shared path policy (D33/G27;
        // the pc5IfPeer default reproduces the SL-2 D16 static rule)
        MacNodeId peerId = NODEID_NONE;
        switch (decidePath(destAddr, typeOfService, &peerId)) {
            case SlPathPolicy::PATH_PC5:
                analyzeUnicastPc5Packet(pkt, peerId);
                return;
            case SlPathPolicy::PATH_DENY:
                // pc5Only: no Uu fallback exists (normally already dropped
                // at SlTechnologyDecision; defensive here)
                EV_WARN << "SlIp2Nic::analyzePacket - dropping unicast to " << destAddr
                        << ": pathSelectionPolicy denies the Uu path" << endl;
                delete pkt;
                return;
            case SlPathPolicy::PATH_UU:
                Ip2Nic::analyzePacket(pkt, srcAddr, destAddr, typeOfService);
                return;
        }
        return;  // unreachable: the switch covers every Decision
    }

    EV << "SlIp2Nic::analyzePacket - PC5 packet, dest=" << destAddr << " -> dstL2Id=" << dstL2Id << endl;

    const SlrbConfigEntry *slrb = slRrc_->getPreconfig().findSlrbForDstL2Id(dstL2Id);
    if (slrb == nullptr)
        throw cRuntimeError("SlIp2Nic: destination L2 ID %u has no slrbConfig entry", dstL2Id);

    MacNodeId dstPid = slBinder_->getOrAssignGroupL2Pid(dstL2Id);

    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    lteInfo->setDirection(SL);
    lteInfo->setSourceId(nrNodeId_);
    lteInfo->setDestId(dstPid);
    lteInfo->setDrbId(slrb->drbId);

    // genie establishment of the SLRB chains at the sender and all receivers
    // on first use (the PDCP entity registry is authoritative). With SL-SDAP
    // the final SLRB is only known after the PFI mapping: establishment
    // moves to onSdapTxReturn.
    if (!hasSlSdap_ && bearerManagement_->lookupPdcpTxEntity(DrbKey(dstPid, slrb->drbId)) == nullptr)
        slBinder_->establishSlConnection(makeSlFlowId(dstPid, slrb->drbId), makeSlBearerRequest(*slrb));
}

void SlIp2Nic::analyzeUnicastPc5Packet(Packet *pkt, MacNodeId peerId)
{
    // establish (or fetch) the PC5 unicast link: synchronous under genie;
    // over the air (D23) the link parks in ESTABLISHING and the packet is
    // held by SlRrc until the handshake completes. All link SLRBs come up
    // with the link, so the SDAP mapping (D20) never establishes anything.
    const SlUnicastLink& link = slRrc_->establishLink(peerId);
    if (link.state != SlUnicastLink::ESTABLISHED) {
        slRrc_->holdPacket(peerId, pkt);
        packetHeld_ = true;
        return;
    }

    // without SL-SDAP every packet rides the link's default SLRB; with it,
    // the drb/rlcType stamped here are provisional until the PFI mapping
    const SlrbConfigEntry& slrb = link.findSlrbForPfi(0);

    EV << "SlIp2Nic::analyzeUnicastPc5Packet - PC5 unicast packet to peer " << peerId
       << " (dstL2Id=" << slrb.dstL2Id << ", drb " << num(slrb.drbId) << ")" << endl;

    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    lteInfo->setDirection(SL);
    lteInfo->setSourceId(nrNodeId_);
    lteInfo->setDestId(peerId);
    lteInfo->setDrbId(slrb.drbId);
}

FlowId SlIp2Nic::makeSlFlowId(MacNodeId dstPid, DrbId drbId) const
{
    FlowId flow;
    flow.direction = SL;
    flow.sourceId = nrNodeId_;
    flow.destId = dstPid;
    flow.drbId = drbId;
    return flow;
}

BearerRequest SlIp2Nic::makeSlBearerRequest(const SlrbConfigEntry& slrb)
{
    BearerRequest req;
    req.qosClass = BACKGROUND;
    req.rlcType = slrb.rlcType;
    req.slCastType = slrb.castType;
    req.slPqi = slrb.pqi;
    return req;
}

int SlIp2Nic::resolvePfi(Packet *pkt)
{
    // resolution chain (D20): explicit QfiReq tag -> DSCP fallback -> default
    if (pkt->hasTag<QfiReq>())
        return (int)pkt->getTag<QfiReq>()->getQfi();
    if (useDscpAsPfiFallback_) {
        auto ipHeader = pkt->peekAtFront<Ipv4Header>();
        uint8_t tos = (uint8_t)ipHeader->getTypeOfService();
        if (tos > 0)
            return tos >> 2;
    }
    return 0;
}

void SlIp2Nic::routeViaSdapTx(Packet *pkt, uint32_t dstL2Id)
{
    auto it = sdapTxGates_.find(dstL2Id);
    if (it == sdapTxGates_.end())
        it = sdapTxGates_.emplace(dstL2Id, bearerManagement_->createSlSdapEntity(true, dstL2Id, this)).first;
    send(pkt, "sdapTxOut", it->second);
}

void SlIp2Nic::routeViaSdapRx(Packet *pkt, uint32_t srcPid)
{
    auto it = sdapRxGates_.find(srcPid);
    if (it == sdapRxGates_.end())
        it = sdapRxGates_.emplace(srcPid, bearerManagement_->createSlSdapEntity(false, srcPid, this)).first;
    send(pkt, "sdapRxOut", it->second);
}

void SlIp2Nic::onSdapTxReturn(Packet *pkt)
{
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    // establish the (now final) SLRB for broadcast/groupcast destinations;
    // unicast link SLRBs were all established with the link
    SlL2Id dstL2Id = slBinder_->getL2IdForNodeId(lteInfo->getDestId());
    const SlrbConfigEntry *slrb = slRrc_->getPreconfig().findSlrbForDstL2Id(dstL2Id);
    if (slrb != nullptr && slrb->castType != SL_UNICAST
        && bearerManagement_->lookupPdcpTxEntity(DrbKey(lteInfo->getDestId(), lteInfo->getDrbId())) == nullptr)
        slBinder_->establishSlConnection(makeSlFlowId(lteInfo->getDestId(), lteInfo->getDrbId()),
                makeSlBearerRequest(*slrb));

    send(pkt, stackGateOut_);
}

void SlIp2Nic::onSdapRxReturn(Packet *pkt)
{
    // continue the base stackIn processing: strip the stack tags, hand to IP
    pkt->removeTagIfPresent<SocketInd>();
    removeAllSimu5GTags(pkt);
    toIpUe(pkt);
}

void SlIp2Nic::resumeHeldPacket(Packet *pkt)
{
    Enter_Method_Silent("resumeHeldPacket()");
    take(pkt);
    // the stale classification of the held packet is re-done from scratch:
    // the link is ESTABLISHED now, so the packet takes the normal path
    pkt->removeTagIfPresent<FlowControlInfo>();
    toStackUe(pkt);
}

} // namespace simu5g
