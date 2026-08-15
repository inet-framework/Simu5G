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

#ifndef __IP2NIC_H_
#define __IP2NIC_H_

#include <inet/common/ModuleRefByPar.h>
#include <inet/networklayer/common/NetworkInterface.h>
#include <set>
#include <unordered_map>
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

using namespace omnetpp;

class HandoverX2Forwarder;


/**
 *
 */
class Ip2Nic : public cSimpleModule
{
  protected:
    RanNodeType nodeType_;      // UE or NODEB

    // reference to the binder
    inet::ModuleRefByPar<Binder> binder_;

    // LTE MAC node id of this node
    MacNodeId nodeId_ = NODEID_NONE;
    // NR MAC node id of this node (if enabled)
    MacNodeId nrNodeId_ = NODEID_NONE;

    // Enable for dual connectivity
    bool dualConnectivityEnabled_;

    // Flag mirroring PDCP's (to be verified with ASSERTs, then used to replace PDCP dependency)
    bool isNr_ = false;
    bool hasSdap_ = false;
    bool establishBearersOnDemand_ = true;

    // Fills in an outgoing packet's FlowControlInfo -- its endpoints, direction and the
    // DRB carrying its flow -- so the stack below can route it. Not free of consequences:
    // a flow that no bearer carries yet gets one established (see
    // establishBearerOnDemand()). Core handles the plain UL/DL path (LTE) and the NR
    // (non-D2D) path; the D2D-aware overrides live in Ip2NicD2D.
    virtual void attachFlowControlInfo(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, uint16_t typeOfService);

    // Fills in the flow's endpoint ids: this node on the near side, and on the far side
    // the next hop towards the destination (the multicast group's sender, for multicast).
    virtual void assignEndpointIds(FlowControlInfo *lteInfo, const inet::Ipv4Address& destAddr, bool useNR, bool isEnb);

    // Establishes a bearer for a flow that has none, and returns the DRB id it got. This
    // is the data plane asking RRC for a bearer, so it is the packet path's one
    // control-plane action -- it builds entities at BOTH endpoints. Throws instead when
    // the establishBearersOnDemand parameter turned this fallback off.
    virtual DrbId establishBearerOnDemand(const FlowBindingKey& key, FlowControlInfo *lteInfo, cPacket *pkt);

    // Which bearer carries which flow: the classifier's half of a bearer, and the only
    // bearer state this module holds. Entirely maintained by RRC, which installs an
    // entry at each endpoint of a bearer it establishes and drops it again when it
    // tears that bearer down -- so a flow is bound exactly while a bearer carries it,
    // and an unbound flow is one that needs a bearer established.
    std::unordered_map<FlowBindingKey, DrbKey, FlowBindingKeyHash> flowBindings_;

    // UEs whose context this node released after a radio link failure (RLF).
    // While a peer is listed here, its DL (gNB) / UL (UE) packets are dropped at
    // Ip2Nic -- modeling RRC UE Context Release: the bearer is gone, so incoming
    // data is discarded rather than pushed at a torn-down entity. RRC (BearerManagement)
    // owns the release/re-establishment lifecycle and toggles this set via releaseUe()/resumeUe().
    std::set<MacNodeId> releasedUes_;

    cGate *stackGateOut_ = nullptr;       // gate connecting Ip2Nic module to cellular stack
    cGate *ipGateOut_ = nullptr;          // gate connecting Ip2Nic module to network layer

    // corresponding entry for our interface
    opp_component_ptr<inet::NetworkInterface> networkIf;

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

    virtual void prepareForIpv4(inet::Packet *datagram, const inet::Protocol *protocol = & inet::Protocol::ipv4);
    virtual void toIpUe(inet::Packet *datagram);
    virtual void toIpBs(inet::Packet *datagram);
    virtual void toStackBs(inet::Packet *datagram);
    virtual void toStackUe(inet::Packet *datagram);

    /// classifies the connection of an outgoing packet (called after the
    /// common preamble of attachFlowControlInfo(), before the endpoint ids are
    /// assigned). No-op in the base; D2D-aware subclasses set the multicast
    /// group, peer IDs and the actual flow direction here.
    virtual void classifyConnection(inet::Packet *pkt, FlowControlInfo *lteInfo, const inet::Ipv4Address& destAddr, MacNodeId localNodeId, bool isEnb) {}

    /// direction stored in the flow key. The plain-LTE stack has historically used
    /// a direction-agnostic key; the NR and D2D stacks key by the actual flow
    /// direction.
    virtual Direction bindingDirection(FlowControlInfo *lteInfo) { return isNr_ ? (Direction)lteInfo->getDirection() : Direction(0xFFFF); }
    virtual MacNodeId getNextHopNodeId(const inet::Ipv4Address& destAddr, bool useNR, MacNodeId sourceId);
    virtual LteTrafficClass getTrafficCategory(cPacket *pkt);

  public:
    // Configuration push: RRC binds a flow to the bearer carrying it, at both endpoints
    // of the bearer it establishes. First binding wins, so a flow already bound keeps
    // its bearer -- which is what makes a dual-connectivity UE keep the master-anchored
    // bearer its packets are routed to. Ip2Nic does not author bindings; together with
    // releaseFlowBindings() this is the only write path.
    virtual void configureFlowBinding(const FlowBindingKey& key, DrbKey bearer);

    // Unbind every flow the given bearer carried: RRC calls this where it tears the
    // bearer down, so the bindings never outlive the entities behind them and the
    // affected flows establish a fresh bearer on their next packet.
    virtual void releaseFlowBindings(DrbKey bearer);

    // Radio link failure handling is data-plane only here: BearerManagement (RRC) drives
    // the release/re-establishment lifecycle and gates this node's packet dropping via
    // the two calls below.

    // Start dropping a peer's future DL/UL packets (UE Context Release). Its bearer
    // re-establishes on demand once resumeUe() is called.
    virtual void releaseUe(MacNodeId ueId);

    // Stop dropping the peer's packets (RRC re-establishment complete); traffic resumes.
    virtual void resumeUe(MacNodeId ueId);
};

} //namespace

#endif
