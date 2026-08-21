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

    // QoS-derived RAN defaults for definitions that state no rlcType/lcg themselves:
    // the amPerThreshold and lcgPriorityBounds parameters (see the NED documentation)
    double amPerThreshold_ = 0;
    std::vector<long> lcgPriorityBounds_;

    // The DRB IDs currently in use within each node pair (see assignDrbId())
    std::map<std::pair<MacNodeId, MacNodeId>, std::set<DrbId>> drbIdsInUse_;

    // A bearer definition retained for establishment-time authoring: the UE it belongs
    // to, the descriptor delivered to the RRCs, and its compiled packet filters. One
    // record per (entry x matched UE); records keep table order, staticDrbs before
    // onDemandDrbs, which is the match order of establishOnDemandBearer(). An
    // onDemandDrbs record carries no DRB id of its own (desc.key stays DRBID_NONE):
    // like every DRB id, an on-demand bearer's identity is pair-scoped, assigned at the
    // definition's first match within each node pair (pairIds) and returned to the
    // pair's pool with the bearer (see forgetOnDemandDrbId()) -- so a UE that moves to
    // another serving node materializes the definition afresh there.
    struct AuthoredBearer {
        cModule *ueModule = nullptr;
        DrbDesc desc;                  // key = (NODEID_NONE, drbId); DRBID_NONE for onDemand
        bool onDemand = false;         // true = onDemandDrbs entry (ids assigned at first match, per pair)
        std::vector<std::unique_ptr<inet::PacketFilter>> filters;   // compiled desc.filters
        std::map<std::pair<MacNodeId, MacNodeId>, DrbId> pairIds;   // onDemand only: id per materialized node pair
    };
    std::vector<AuthoredBearer> authoredBearers_;


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
    // Ip2Nic supplies the flow, its classifier key and the triggering packet, and the
    // bearer's properties come from the "eps" definition whose packet filter matches
    // (staticDrbs first, then onDemandDrbs, in table order; the default entry catches
    // what no filter matched). A flow no definition covers throws: the onDemandDrbs
    // default value carries catch-all definitions, so only a configuration that
    // replaced them with a non-covering set can get here. D2D and multicast bearers
    // are outside the definition system and get a fixed transitional configuration
    // (RLC UM, LCG 3). Returns the established bearer's DRB id.
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

    // True if a staticDrbs entry of the UE names this DRB id. A static definition owns
    // its id for the whole run: releasing it would let assignDrbId() hand the id to an
    // unrelated bearer while the definition still names it, so teardown must skip it.
    virtual bool ownsStaticDrbId(cModule *ueModule, DrbId drbId);

    // Forget an on-demand definition's materialization in the given node pair when its
    // bearer is torn down: the id has returned to the pair's pool (releaseDrbId()), and
    // the next matching flow assigns afresh. Forgetting an id that is not recorded is a
    // no-op, so both endpoints may call it.
    virtual void forgetOnDemandDrbId(cModule *ueModule, MacNodeId a, MacNodeId b, DrbId drbId);
};

} //namespace

#endif
