//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//
#include <inet/common/socket/SocketTag_m.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include <inet/linklayer/common/InterfaceTag_m.h>
#include "simu5g/stack/ip2nic/Ip2Nic.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(Ip2Nic);

void Ip2Nic::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        stackGateOut_ = gate("stackOut");
        ipGateOut_ = gate("upperLayerOut");

        nodeType_ = aToNodeType(par("nodeType").stdstringValue());

        binder_.reference(this, "binderModule", true);
        smf_.reference(this, "smfModule", true);

        networkIf = getContainingNicModule(this);
        dualConnectivityEnabled_ = networkIf->par("dualConnectivityEnabled").boolValue();

        if (nodeType_ == NODEB) {
            cModule *bs = getContainingNode(this);
            nodeId_ = MacNodeId(bs->par("macNodeId").intValue());
        }
        else if (nodeType_ == UE) {
            cModule *ue = getContainingNode(this);
            nodeId_ = MacNodeId(ue->par("macNodeId").intValue());
            if (ue->hasPar("nrMacNodeId"))
                nrNodeId_ = MacNodeId(ue->par("nrMacNodeId").intValue());
        }
    }
    else if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS) {
        isNr_ = par("isNr");

        hasSdap_ = par("hasSdap").boolValue();
        establishBearersOnDemand_ = par("establishBearersOnDemand").boolValue();
    }
}

void Ip2Nic::handleMessage(cMessage *msg)
{
    if (msg->getArrivalGate()->isName("upperLayerIn")) {
        if (nodeType_ == NODEB)
            toStackBs(check_and_cast<Packet*>(msg));
        else {
            toStackUe(check_and_cast<Packet*>(msg));
        }
    }
    else if (msg->getArrivalGate()->isName("stackIn")) {
        EV << "Ip2Nic: message from stack: sending up" << endl;
        auto pkt = check_and_cast<Packet *>(msg);
        pkt->removeTagIfPresent<SocketInd>();
        removeAllSimu5GTags(pkt);
        if (nodeType_ == NODEB)
            toIpBs(pkt);
        else
            toIpUe(pkt);
    }
    else {
        throw cRuntimeError("Message received on wrong gate %s", msg->getArrivalGate()->getFullName());
    }
}

void Ip2Nic::releaseUe(MacNodeId ueId)
{
    Enter_Method_Silent();
    EV << NOW << " Ip2Nic::releaseUe - releasing context for node " << ueId
       << " (dropping its future DL/UL traffic)" << endl;
    releasedUes_.insert(ueId);
    // Nothing to purge here: the teardown that follows notifies this module bearer by
    // bearer (see bearerReleased()), so the peer's next packet -- once resumeUe() lets
    // it through again -- re-establishes on its own.
}

void Ip2Nic::resumeUe(MacNodeId ueId)
{
    Enter_Method_Silent();
    EV << NOW << " Ip2Nic::resumeUe - resuming traffic for node " << ueId
       << " (RRC re-establishment complete)" << endl;
    releasedUes_.erase(ueId);
}

void Ip2Nic::toStackUe(Packet *pkt)
{
    EV << "Ip2Nic::fromIpUe - message from IP layer: send to stack: " << pkt->str() << std::endl;
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    auto srcAddr = ipHeader->getSrcAddress();
    auto destAddr = ipHeader->getDestAddress();
    short int tos = ipHeader->getTypeOfService();

    // Drop UL packets if this UE released its link to the serving node after RLF.
    if (!releasedUes_.empty()) {
        if (releasedUes_.count(binder_->getServingNodeOrSelf(nodeId_)) ||
            (nrNodeId_ != NODEID_NONE && releasedUes_.count(binder_->getServingNodeOrSelf(nrNodeId_)))) {
            EV << "Ip2Nic::toStackUe - link released (RLF); dropping UL packet" << endl;
            delete pkt;
            return;
        }
    }

    // TODO: Add support for IPv6 (=> see L3Tools.cc of INET)

    // TechnologyReq tag (useNR) is already set by TechnologyDecision

    attachFlowControlInfo(pkt, srcAddr, destAddr, tos);
    if (!hasSdap_)   // with SDAP, it maps the QoS flow onto a DRB itself
        assignBearer(pkt, srcAddr, destAddr, tos);

    // Send datagram to LTE stack or LteIp peer
    send(pkt, stackGateOut_);
}

void Ip2Nic::prepareForIpv4(Packet *datagram, const Protocol *protocol) {
    // add DispatchProtocolRequest so that the packet is handled by the specified protocol
    datagram->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(protocol);
    datagram->addTagIfAbsent<PacketProtocolTag>()->setProtocol(protocol);
    // add Interface-Indication to indicate which interface this packet was received from
    datagram->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkIf->getInterfaceId());
}

void Ip2Nic::toIpUe(Packet *pkt)
{
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    auto networkProtocolInd = pkt->addTagIfAbsent<NetworkProtocolInd>();
    networkProtocolInd->setProtocol(&Protocol::ipv4);
    networkProtocolInd->setNetworkProtocolHeader(ipHeader);
    prepareForIpv4(pkt);
    EV << "Ip2Nic::toIpUe - message from stack: send to IP layer" << endl;
    send(pkt, ipGateOut_);
}

void Ip2Nic::toIpBs(Packet *pkt)
{
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    auto networkProtocolInd = pkt->addTagIfAbsent<NetworkProtocolInd>();
    networkProtocolInd->setProtocol(&Protocol::ipv4);
    networkProtocolInd->setNetworkProtocolHeader(ipHeader);
    prepareForIpv4(pkt, &LteProtocol::ipv4uu);
    EV << "Ip2Nic::toIpBs - message from stack: send to IP layer" << endl;
    send(pkt, ipGateOut_);
}

void Ip2Nic::toStackBs(Packet *pkt)
{
    EV << "Ip2Nic::toStackBs - message from IP layer: send to stack" << endl;
    removeAllSimu5GTags(pkt);
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    auto srcAddr = ipHeader->getSrcAddress();
    auto destAddr = ipHeader->getDestAddress();
    short int tos = ipHeader->getTypeOfService();

    // Drop DL packets destined to a UE whose context was released after RLF
    // (UE Context Release: discard rather than push at a torn-down bearer).
    if (!releasedUes_.empty()) {
        if (releasedUes_.count(binder_->getMacNodeId(destAddr)) ||
            releasedUes_.count(binder_->getNrMacNodeId(destAddr))) {
            EV << "Ip2Nic::toStackBs - UE context released (RLF); dropping DL packet for " << destAddr << endl;
            delete pkt;
            return;
        }
    }

    // TechnologyReq tag (useNR) is already set by TechnologyDecision

    attachFlowControlInfo(pkt, srcAddr, destAddr, tos);
    if (!hasSdap_)   // with SDAP, it maps the QoS flow onto a DRB itself
        assignBearer(pkt, srcAddr, destAddr, tos);

    send(pkt, stackGateOut_);
}

void Ip2Nic::configureFlowBinding(const FlowBindingKey& key, DrbKey bearer)
{
    Enter_Method_Silent("configureFlowBinding()");
    EV << "Ip2Nic::configureFlowBinding - flow " << key.srcAddr << " -> " << key.dstAddr
       << " (ToS=" << key.typeOfService << ") is carried by " << bearer << endl;
    flowBindings_.emplace(key, bearer);  // no-op if the flow is already bound
}

void Ip2Nic::releaseFlowBindings(DrbKey bearer)
{
    Enter_Method_Silent("releaseFlowBindings()");
    for (auto it = flowBindings_.begin(); it != flowBindings_.end(); ) {
        if (it->second == bearer) {
            EV << "Ip2Nic::releaseFlowBindings - flow " << it->first.srcAddr << " -> "
               << it->first.dstAddr << " (ToS=" << it->first.typeOfService << ") unbound from " << bearer << endl;
            it = flowBindings_.erase(it);
        }
        else
            ++it;
    }
}

MacNodeId Ip2Nic::getNextHopNodeId(const Ipv4Address& destAddr, bool useNR, MacNodeId sourceId)
{
    bool isEnb = (nodeType_ == NODEB);

    if (isEnb) {
        // ENB variants
        MacNodeId destId;
        if (isNr_ && (!dualConnectivityEnabled_ || useNR))
            destId = binder_->getNrMacNodeId(destAddr);
        else
            destId = binder_->getMacNodeId(destAddr);

        // master of this UE (myself)
        MacNodeId master = binder_->getServingNodeOrSelf(destId);
        if (master != nodeId_) {
            destId = master;
        }
        else {
            // for dual connectivity
            master = binder_->getMasterNodeOrSelf(master);
            if (master != nodeId_) {
                destId = master;
            }
        }
        // else UE is directly attached
        return destId;
    }
    else {
        // UE variants (no D2D): the packet goes to the serving node; the UE is subject
        // to handovers, so the master may change. For LTE that node is nodeId_'s master;
        // for NR it is the master of the (technology-selected) source node passed in.
        if (!isNr_)
            return binder_->getServingNodeOrSelf(nodeId_);
        return binder_->getServingNodeOrSelf(sourceId);
    }
}

void Ip2Nic::attachFlowControlInfo(inet::Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    // --- Common preamble ---
    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();

    // direction of transmitted packets depends on node type
    Direction dir = (nodeType_ == UE) ? UL : DL;
    lteInfo->setDirection(dir);

    // the IP header stops being reachable below PDCP, so the marking travels with the flow
    lteInfo->setTypeOfService(typeOfService);

    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();
    bool isEnb = (dir == DL);

    // For NrPdcpUe, the effective local node ID depends on the useNR flag
    MacNodeId localNodeId = (isNr_ && !isEnb) ? (useNR ? nrNodeId_ : nodeId_) : nodeId_;

    if (isEnb || !isNr_)
        EV << "Received packet from data port, src= " << srcAddr << " dest=" << destAddr << " ToS=" << typeOfService << endl;

    // D2D-aware subclasses set the multicast group, peer IDs and the actual
    // flow direction here; no-op otherwise
    classifyConnection(pkt, lteInfo.get(), destAddr, localNodeId, isEnb);

    assignEndpointIds(lteInfo.get(), destAddr, useNR, isEnb);
}

void Ip2Nic::assignBearer(inet::Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    // TODO: Since IP addresses can change when we add and remove nodes, maybe node IDs should be used instead of them
    FlowBindingKey key{srcAddr, destAddr, typeOfService, bindingDirection(lteInfo.get())};
    auto it = flowBindings_.find(key);
    // A flow is bound exactly while a bearer carries it: an unbound one is either new,
    // or its bearer was torn down (at a handover, say), which unbinds it.
    DrbId drbId = (it != flowBindings_.end()) ? it->second.getDrbId()
                                              : establishBearerOnDemand(key, lteInfo.get(), pkt);
    lteInfo->setDrbId(drbId);

    EV << "Ip2Nic::assignBearer - flow " << srcAddr << " -> " << destAddr
       << " (ToS=" << typeOfService << ") is carried by DRB " << drbId << endl;
}

void Ip2Nic::assignEndpointIds(FlowControlInfo *lteInfo, const Ipv4Address& destAddr, bool useNR, bool isEnb)
{
    if (isNr_) {
        // For PDCP entity dispatch, always use technology-neutral (LTE/master-leg) IDs.
        // The TechnologyReq::useNR flag carries the LTE-vs-NR routing decision separately;
        // NrPdcpTxEntity reads it in deliverPdcpPdu() to decide local RLC vs X2 forwarding.
        if (isEnb) {
            lteInfo->setSourceId(nodeId_);
            if (lteInfo->getMulticastGroupId() != NODEID_NONE)
                lteInfo->setDestId(nodeId_);
            else
                lteInfo->setDestId(getNextHopNodeId(destAddr, !dualConnectivityEnabled_ && useNR, nodeId_));
        }
        else {
            // UE: use LTE UE ID when DC is enabled (both legs share one PDCP entity),
            // NR UE ID when non-DC NR (entity was created with NR IDs)
            MacNodeId ueSourceId = (dualConnectivityEnabled_ ? nodeId_ : (useNR ? nrNodeId_ : nodeId_));
            lteInfo->setSourceId(ueSourceId);
            if (lteInfo->getMulticastGroupId() != NODEID_NONE)
                lteInfo->setDestId(nodeId_);
            else
                lteInfo->setDestId(getNextHopNodeId(destAddr, !dualConnectivityEnabled_ && useNR, ueSourceId));
        }
    }
    else {
        // This NIC has no NR leg, so there is no NR id to select and useNR does not
        // apply: getNextHopNodeId() consults it only when isNr_ is set. Every NR-capable
        // NIC takes the branch above, a dual-connectivity master eNB included (it sets
        // isNr from the ini).
        lteInfo->setSourceId(nodeId_);
        if (lteInfo->getMulticastGroupId() != NODEID_NONE)  // destId is meaningless for multicast D2D (we use the id of the source for statistic purposes at lower levels)
            lteInfo->setDestId(nodeId_);
        else
            lteInfo->setDestId(getNextHopNodeId(destAddr, false, lteInfo->getSourceId()));
    }
}

DrbId Ip2Nic::establishBearerOnDemand(const FlowBindingKey& key, FlowControlInfo *lteInfo, inet::Packet *pkt)
{
    if (!establishBearersOnDemand_)
        throw cRuntimeError("Ip2Nic: no established bearer for flow %s -> %s (ToS=%d), and on-demand bearer establishment is disabled",
                key.srcAddr.str().c_str(), key.dstAddr.str().c_str(), (int)key.typeOfService);

    FlowId flow = lteInfo->toFlowId();
    flow.drbId = DRBID_NONE;   // a new bearer, whose id the establishment assigns

    // The flow key travels with the request: RRC binds the flow to the bearer at both
    // endpoints (see configureFlowBinding), so this node's own binding and the peer's
    // mirrored one are installed by the same establishment. The packet is what the
    // SMF authors the bearer's properties from.
    return smf_->establishOnDemandBearer(flow, key, pkt);
}

} //namespace
