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

#ifndef _SMF_H_
#define _SMF_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/PacketFilter.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/rrc/DrbDesc.h"

namespace simu5g {

using namespace omnetpp;

/**
 * The Session Management Function of the core network. It has one instance in
 * the whole network, and it is what decides which data radio bearers exist and
 * when they are set up. See the NED file for details.
 */
class Smf : public cSimpleModule
{
  public:
    ~Smf() override;

  protected:
    inet::ModuleRefByPar<Binder> binder_;

    // The DRB IDs currently in use within each node pair (see assignDrbId())
    std::map<std::pair<MacNodeId, MacNodeId>, std::set<DrbId>> drbIdsInUse_;

    // A bearer definition retained for establishment-time authoring: the UE it belongs
    // to, the descriptor delivered to the RRCs, and its compiled packet filters. One
    // record per (entry x matched UE); records keep table order, staticDrbs before
    // onDemandDrbs, which is the match order of establishOnDemandBearer(). An
    // onDemandDrbs record has no DRB id (DRBID_NONE) until its first match assigns one.
    struct AuthoredBearer {
        cModule *ueModule = nullptr;
        DrbDesc desc;                  // key = (NODEID_NONE, drbId)
        bool onDemand = false;         // true = onDemandDrbs entry (id assigned at first match)
        std::vector<std::unique_ptr<inet::PacketFilter>> filters;   // compiled desc.filters
    };
    std::vector<AuthoredBearer> authoredBearers_;

    // A fallback-classification rule compiled from the lcgRules parameter:
    // what authors the logical channel group of an on-demand bearer that no bearer
    // definition covers -- including D2D and multicast bearers, which definitions
    // never describe. Authors a property only: flows classified here still mint
    // their own bearers, they never share one the way definition-matched flows do.
    // The parameter's default value carries the legacy packet-name mapping
    // ("VoIP*" = LCG 0, ...).
    struct LcgRule {
        std::unique_ptr<inet::PacketFilter> filter;   // null = match all
        Lcg lcg = Lcg(3);
    };
    std::vector<LcgRule> lcgRules_;

    // The RLC mode of the bearers the fallback rules classify (definition entries state
    // their own); the defaultRlcType parameter. Transitional, like lcgRules_.
    LteRlcType defaultRlcType_ = UM;

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override { throw cRuntimeError("This module does not process messages"); }

    // Configure the data radio bearers described by the staticDrbs parameter (see NED
    // documentation) by telling the RRC of each node involved in a bearer what to set
    // up: BearerManagement::configureDrb() at the UE and at its serving node. The
    // configuration is pushed from here, and no module ever reads it back out.
    virtual void configureDrbs();

    // Establish the bearers described by the staticBearers parameter (see NED
    // documentation), in the last initialization stage. Each entry goes through
    // establishDataConnection(), exactly like packet-triggered establishment.
    virtual void establishStaticBearers();

    // Parse and compile the lcgRules parameter; errors throw at setup.
    virtual void parseLcgRules();

    // First-match-wins over lcgRules_; LCG 3 -- the group of the non-GBR default
    // bearer -- when no rule matches.
    virtual Lcg classifyLcg(const inet::Packet *pkt);

    // Establish the flow on the bearer a definition describes, assigning the
    // definition its DRB id and delivering it to the RRCs first if it does not
    // have one yet (i.e. an onDemandDrbs entry matched for the first time).
    virtual DrbId establishFromDefinition(AuthoredBearer& ab, const FlowId& flow, const FlowBindingKey& key);

    // The definition a flow's bearer was authored from, or nullptr if none covers it
    virtual const DrbDesc *findBearerDefinition(const FlowId& flow);

    // The standardized QoS characteristics rows as built-in drbProfiles entries
    // ("qci-1".."qci-9", "5qi-1".."5qi-9"); built lazily, owned by this module
    omnetpp::cValueMap *predefinedDrbProfiles_ = nullptr;
    virtual const omnetpp::cValueMap *getPredefinedDrbProfiles();


    // Deliver one bearer's definition to the RRCs involved: the UE's (keyed by
    // NODEID_NONE, "my serving node") and, for each attached stack, the serving
    // node's (keyed by that stack's UE id), reserving the configured id per pair.
    virtual void pushDrbToRrcs(cModule *ueModule, const DrbDesc& drb);

    // Parse one bearer-definition table (staticDrbs or onDemandDrbs) into
    // authoredBearers_, and, for staticDrbs, into drbsOfUe for the init-time pushes.
    virtual void parseDrbDefinitions(const char *paramName, bool onDemand,
            const std::map<cModule *, std::vector<MacNodeId>>& ueNodeIds, const std::string& networkPrefix,
            std::map<cModule *, std::map<DrbId, DrbDesc>>& drbsOfUe);

    virtual bool isDualConnectivityRequired(const FlowId& flow);
    virtual void createConnection(const FlowId& flow, const BearerRequest& req, bool withPdcp);
    virtual void createIncomingConnectionOnNode(MacNodeId nodeId, const FlowId& flow, const BearerRequest& req, bool withPdcp);
    virtual void createOutgoingConnectionOnNode(MacNodeId nodeId, const FlowId& flow, const BearerRequest& req, bool withPdcp);

  public:
    // Establish a duplex data radio bearer for the flow described by lteInfo:
    // entities for BOTH directions are created at both endpoints at once (DRBs are
    // bidirectional per TS 38.331; RLC-AM in particular needs the reverse path for
    // its STATUS PDUs). Multicast flows remain unidirectional (TX at the sender,
    // RX at the group members).
    //
    // The bearer's DRB id is the flow's own when it has one, and a freshly assigned
    // one (see assignDrbId()) when flow.drbId is DRBID_NONE, i.e. when the requester
    // is establishing a bearer for a flow it has not seen before. Either way the id
    // of the established bearer is returned.
    virtual DrbId establishDataConnection(const FlowId& flow, const BearerRequest& req);

    // Establish a bearer for a flow the requester identifies but does not describe:
    // Ip2Nic supplies the flow, its classifier key and the triggering packet, and this
    // method authors the bearer's properties. An "eps" bearer definition whose packet
    // filter matches (staticDrbs first, then onDemandDrbs, in table order; the default
    // entry catches what no filter matched) supplies them; a flow no definition covers
    // falls back to the LCG the lcgRules derive from the packet, with the RLC mode
    // filled from defaultRlcType. Returns the established bearer's DRB id.
    virtual DrbId establishOnDemandBearer(const FlowId& flow, const FlowBindingKey& key, const inet::Packet *pkt);

    // Create the DRB serving the given QFI at the given UE from a matching "5gc"
    // onDemandDrbs definition: the id is assigned, and the definition is delivered to
    // the RRCs involved exactly like a staticDrbs entry (so it also reaches SDAP's
    // QFI-to-DRB table). Returns the DRB id, or DRBID_NONE when no definition covers
    // the QFI (or the UE is not attached). Called by SDAP on a QFI-to-DRB lookup miss;
    // repeated calls return the already-created DRB.
    virtual DrbId createOnDemandDrbForQfi(MacNodeId ueNodeId, Qfi qfi);

    // Allocate the lowest free DRB ID within the (unordered) node pair {a, b}, so the
    // two endpoints of a link can never mint colliding IDs for the same peer.
    // For multicast flows, pass the multicast group ID as the second node.
    virtual DrbId assignDrbId(MacNodeId a, MacNodeId b);

    // Mark an externally chosen DRB ID as in use, so assignDrbId() cannot hand out the
    // same one later (SDAP and the staticBearers entries name their bearers themselves).
    virtual void reserveDrbId(MacNodeId a, MacNodeId b, DrbId drbId);

    // Return a DRB ID to its pair's pool when the bearer is torn down. DRB identities are
    // a finite per-UE resource (TS 38.331: DRB-Identity is 1..32) and are reused once
    // released -- without this, a UE handing over repeatedly would exhaust the space.
    // Releasing an ID that is not in use is a no-op, so both endpoints may call it.
    virtual void releaseDrbId(MacNodeId a, MacNodeId b, DrbId drbId);
};

} //namespace

#endif
