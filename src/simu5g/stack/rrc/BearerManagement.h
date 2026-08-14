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

#ifndef _BEARER_MANAGEMENT_H_
#define _BEARER_MANAGEMENT_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rrc/DrbTable.h"
#include <inet/common/ModuleRefByPar.h>
#include <set>
#include <utility>

using namespace omnetpp;

namespace simu5g {

class LteMacBase;
class RlcMux;
class RlcTxEntityBase;
class RlcRxEntityBase;
class RlcUmTxEntityBase;
class PdcpMux;
class DcMux;
class PdcpTxEntityBase;
class PdcpRxEntityBase;
class Registration;
class Binder;
class NrSdap;

/**
 * @brief RRC Bearer Management — creates and tears down PDCP, RLC and MAC
 *        entities for data radio bearers.
 */
class BearerManagement : public cSimpleModule
{
  public:
    /**
     * The protocol leg a bearer is built on. A leg is a (MAC, RlcMux) pair plus the
     * entity profiles belonging to it; the NIC hosts up to three side by side (see
     * ~NrNicUe): the LTE Uu leg, the NR Uu leg, and the optional sidelink (PC5) leg.
     * Each leg keeps its own entity registry, so Uu handover and detach teardown
     * never reaches a sidelink bearer.
     */
    enum StackLeg { LEG_LTE, LEG_NR, LEG_SL };

  protected:
    Registration *registration_ = nullptr;

    // PDCP entity types (resolved from NED params): the full PDCP entity is a compound
    // (LtePdcpEntity/NrPdcpEntity, TX+RX); at a DC secondary an PdcpRelayEntity stands in for it.
    cModuleType *pdcpEntityModuleType_ = nullptr;
    cModuleType *pdcpRelayEntityModuleType_ = nullptr;
    cModule *nicModule_ = nullptr;  // containing NIC module (parent of all submodules and entities)

    // RLC entity types (compound modules; resolved from NED params):
    // lteRlc* = LTE-FI bearers (TS 36.322), nrRlc* = NR bearers (SI/SO, TS 38.322)
    cModuleType *rlcTmEntityModuleType_ = nullptr;
    cModuleType *lteRlcUmEntityModuleType_ = nullptr;
    cModuleType *lteRlcAmEntityModuleType_ = nullptr;
    cModuleType *nrRlcUmEntityModuleType_ = nullptr;
    cModuleType *nrRlcAmEntityModuleType_ = nullptr;

    inet::ModuleRefByPar<RlcMux> rlcMuxModule;
    inet::ModuleRefByPar<RlcMux> nrRlcMuxModule;
    inet::ModuleRefByPar<LteMacBase> macModule;
    inet::ModuleRefByPar<LteMacBase> nrMacModule;
    // Sidelink leg: entity profiles and modules, resolved lazily because the leg
    // exists only on a node configured with it. SLRBs use the NR RLC profiles --
    // sidelink RLC is TS 38.322 (TR 38.885 5.4.2) -- while PDCP stays on the LTE
    // profile, whose TX side does not rewrite a UE's source id to its NR node id.
    cModuleType *slPdcpEntityModuleType_ = nullptr;
    cModuleType *slRlcUmEntityModuleType_ = nullptr;
    cModuleType *slRlcAmEntityModuleType_ = nullptr;
    cModuleType *slSdapEntityModuleType_ = nullptr;
    RlcMux *slRlcMux_ = nullptr;
    LteMacBase *slMac_ = nullptr;

    inet::ModuleRefByPar<Binder> binderModule;   // DC master/secondary topology lookups; peer BearerManagement on RLF
    inet::ModuleRefByPar<DrbTable> drbTableModule;   // the bearer configuration this module authors
    inet::ModuleRefByPar<NrSdap> sdapModule;     // configuration push target; null when the NIC has no SDAP

    // Entity registries (CP owns the lifecycle of all entities)
    // One PDCP entity module (compound: TX+RX, see PdcpEntityBase) per bearer, keyed by (peer
    // node, DRB id); the tx/rx submodule pointers below index into it for per-side lookups.
    std::map<DrbKey, cModule *> pdcpEntities_;
    std::map<DrbKey, PdcpTxEntityBase *> pdcpTxEntities_;
    std::map<DrbKey, PdcpRxEntityBase *> pdcpRxEntities_;
    // One PdcpRelayEntity compound per DC-secondary bearer (both directions), keyed by (UE, DRB id)
    std::map<DrbKey, cModule *> pdcpRelayEntities_;
    // One RLC entity module (compound: TX+RX sides, see RlcTm/Um/AmEntity) per
    // bearer, keyed by (peer node, DRB id); one map per leg (LTE / NR)
    std::map<DrbKey, cModule *> rlcEntities_;
    std::map<DrbKey, cModule *> nrRlcEntities_;
    std::map<DrbKey, cModule *> slRlcEntities_;
    std::map<DrbKey, cModule *> slPdcpEntities_;

    // Radio Link Failure: teardown is deferred to a safe execution context via a
    // self-message, so we never delete entity modules from inside RLC/PDCP processing.
    cMessage *rlfTrigger_ = nullptr;
    std::set<std::pair<MacNodeId, bool>> pendingRlf_;  // (peer nodeId, nrStack)
    virtual void handleRadioLinkFailure(MacNodeId nodeId, bool nrStack);

    // RRC re-establishment (TS 38.331 5.3.7), modeled by its timers. On RLF the peer's link
    // is released (Ip2Nic drops its traffic); T311 (cell selection) then T301 (request ->
    // complete) run, after which the peer is un-released (Ip2Nic::resumeUe) and its bearer
    // re-establishes on demand. t311_ = 0 => release-to-IDLE (no reconnect attempt).
    simtime_t t311_;
    simtime_t t301_;
    std::map<cMessage *, MacNodeId> t311Timers_;   // pending cell-selection timers -> peer
    std::map<cMessage *, MacNodeId> t301Timers_;   // pending request->complete timers -> peer

    // NR dual connectivity on this NIC: infrastructure bearers get two legs (see
    // findOrCreatePdcpEntity)
    bool dualConnectivityEnabled_ = false;

    // QoS-class -> RLC-mode mapping, applied by qosClassToRlcType() when a bearer request's
    // rlcType is left unresolved (UNKNOWN_RLC_TYPE).
    LteRlcType conversationalRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType streamingRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType interactiveRlc_ = UNKNOWN_RLC_TYPE;
    LteRlcType backgroundRlc_ = UNKNOWN_RLC_TYPE;

    // Maps a traffic class to its configured RLC mode (conversational/streaming/
    // interactive/backgroundRlc NED params).
    virtual LteRlcType qosClassToRlcType(LteTrafficClass qosClass);

    // The authored configuration entry for the bearer of an infrastructure unicast flow
    // (from the drbTable's drbConfig parameter), or nullptr. peerId is the flow's remote
    // end; on the UE side entries are keyed by NODEID_NONE instead.
    virtual const DrbDesc *lookupConfiguredDrb(const FlowId& flow, MacNodeId peerId);

    // Resolves req.rlcType before any use of it (entity type selection, materializeDrb),
    // called once at the top of each establishment entry point. The authored configuration
    // wins (a conflicting explicit request throws); an rlcType still left as
    // UNKNOWN_RLC_TYPE ("RRC decides") falls back to qosClassToRlcType().
    virtual BearerRequest resolveBearerRequest(const BearerRequest& req, const FlowId& flow, MacNodeId peerId);

    virtual void setRlcEntityParams(cModule *entity, StackLeg leg);
    virtual void setEntityDisplayPosition(cModule *entity, bool isPdcpEntity, cModule *rlcMux, int bearerIndex);
    virtual cModule *lookupRlcEntityModule(DrbKey id, StackLeg leg);
    virtual cModule *findOrCreateRlcEntity(DrbKey id, LteRlcType rlcType, const FlowId& flow, RlcMux *rlcMux, StackLeg leg);
    virtual RlcTxEntityBase *installRlcTxSide(DrbKey id, const FlowId& flow, const BearerRequest& req, RlcMux *rlcMux, StackLeg leg);
    virtual RlcRxEntityBase *installRlcRxSide(DrbKey id, const FlowId& flow, const BearerRequest& req, RlcMux *rlcMux, StackLeg leg);
    virtual cModule *findOrCreatePdcpEntity(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg);
    virtual void installPdcpTxSide(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg);
    virtual void installPdcpRxSide(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg);
    virtual cModule *findOrCreatePdcpRelayEntity(DrbKey id, RlcMux *rlcMux);

    // The layout of a bearer over the node's stack legs. getNumLegs() gives the number of legs
    // the bearer's PDCP entity is built with, selectPdcpLeg() the leg an establishment call
    // attaches to (and, for a leg of a bearer anchored elsewhere, the anchor bearer's key).
    virtual int getNumLegs(DrbKey id, const FlowId& flow, StackLeg leg);
    virtual int selectPdcpLeg(StackLeg leg, MacNodeId peerId, DrbKey& compoundId /*inout*/);

    // Records the configuration of the bearer this establishment call sets up, from
    // exactly the derivations the entities are built from. Runs after the RLC entity
    // exists: the wire format and the SN field length are properties of the entity that
    // implements the bearer, and are read off it. Returns the descriptor so callers can
    // push it into MAC without a re-lookup.
    virtual const DrbDesc& materializeDrb(const FlowId& flow, const BearerRequest& req, MacNodeId peerId, DrbKey rlcId, StackLeg leg);

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

  public:
    ~BearerManagement() override;
    // Schedule an RLC-detected radio link failure teardown for a peer node, deferred
    // to a safe execution context. nrStack selects the failing leg (LTE vs NR).
    virtual void scheduleRadioLinkFailure(MacNodeId nodeId, bool nrStack);
    // Release + tear down the link to a peer (both legs, MAC/RLC/PDCP + Ip2Nic drop). Public
    // so the failing node can drive the symmetric teardown on the peer via the binder.
    virtual void releaseLink(MacNodeId peerId);
    virtual void createIncomingConnection(const FlowId& flow, const BearerRequest& req, bool withPdcp=true);
    virtual void createOutgoingConnection(const FlowId& flow, const BearerRequest& req, bool withPdcp=true);
    virtual RlcTxEntityBase *createRlcTxBuffer(DrbKey id, const FlowId& flow, const BearerRequest& req);
    virtual RlcRxEntityBase *createRlcRxBuffer(DrbKey id, const FlowId& flow, const BearerRequest& req);
    virtual RlcTxEntityBase *lookupRlcTxBuffer(DrbKey id);
    virtual PdcpTxEntityBase *lookupPdcpTxEntity(DrbKey id);
    virtual cModule *lookupPdcpEntityModule(DrbKey id);    // the per-bearer PdcpEntityBase compound (DcMux leg dispatch)
    virtual cModule *lookupPdcpRelayEntityModule(DrbKey id); // the per-bearer PdcpRelayEntity compound (DcMux DL dispatch)
    virtual void deleteLocalPdcpEntities(MacNodeId nodeId);
    virtual void deleteLocalRlcQueues(MacNodeId nodeId, bool nrStack=false);
    virtual void pdcpActiveUeUL(std::set<MacNodeId> *ueSet);

    // --- sidelink bearers, built on the PC5 leg ---
    /// Establish one direction of a sidelink bearer. The request carries the PC5
    /// half (cast type, PC5 5QI) that the descriptor and the logical channel
    /// configuration are authored from.
    virtual void createSlOutgoingConnection(const FlowId& flow, const BearerRequest& req);
    virtual void createSlIncomingConnection(const FlowId& flow, const BearerRequest& req);
    /// The reserved transparent-mode bearer carrying PC5-RRC toward a peer. It has
    /// no PDCP: the SlRrc module takes that place, one srbOut/srbIn gate pair per
    /// link. Returns the index of the new pair.
    virtual int createSlSrbConnection(const FlowId& out, const FlowId& in, cModule *slRrcModule);
    /// One SDAP entity per PC5 destination, wired as a side chain off SlIp2Nic;
    /// returns the SlIp2Nic-side gate index of the new pair.
    virtual int createSlSdapEntity(bool tx, uint32_t peerKey, cModule *slIp2Nic);

  protected:
    /// Resolve the sidelink leg's modules and entity profiles on first use.
    virtual void resolveSlModules();
    std::map<DrbKey, cModule *>& rlcEntitiesOf(StackLeg leg);
    std::map<DrbKey, cModule *>& pdcpEntitiesOf(StackLeg leg);
    RlcMux *rlcMuxOf(StackLeg leg);
    static const char *legPrefix(StackLeg leg);
};

} // namespace simu5g

#endif
