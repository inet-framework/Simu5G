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

/**
 * @brief RRC Bearer Management — creates and tears down PDCP, RLC and MAC
 *        entities for data radio bearers.
 */
class BearerManagement : public cSimpleModule
{
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
    inet::ModuleRefByPar<Binder> binderModule;   // DC master/secondary topology lookups; peer BearerManagement on RLF

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

    virtual void setRlcEntityParams(cModule *entity, bool isNr);
    virtual void setEntityDisplayPosition(cModule *entity, bool isPdcpEntity, cModule *rlcMux, int bearerIndex);
    virtual cModule *lookupRlcEntityModule(DrbKey id, bool isNr);
    virtual cModule *findOrCreateRlcEntity(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    virtual RlcTxEntityBase *installRlcTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    virtual RlcRxEntityBase *installRlcRxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    virtual cModule *findOrCreatePdcpEntity(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux);
    virtual void installPdcpTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    virtual void installPdcpRxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    virtual cModule *findOrCreatePdcpRelayEntity(DrbKey id, RlcMux *rlcMux);

    // The layout of a bearer over the node's stack legs. getNumLegs() gives the number of legs
    // the bearer's PDCP entity is built with, selectPdcpLeg() the leg an establishment call
    // attaches to (and, for a leg of a bearer anchored elsewhere, the anchor bearer's key).
    virtual int getNumLegs(DrbKey id, FlowControlInfo *lteInfo);
    virtual int selectPdcpLeg(bool isNr, MacNodeId peerId, DrbKey& compoundId /*inout*/);

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
    virtual void createIncomingConnection(FlowControlInfo *lteInfo, bool withPdcp=true);
    virtual void createOutgoingConnection(FlowControlInfo *lteInfo, bool withPdcp=true);
    virtual RlcTxEntityBase *createRlcTxBuffer(DrbKey id, FlowControlInfo *lteInfo);
    virtual RlcRxEntityBase *createRlcRxBuffer(DrbKey id, FlowControlInfo *lteInfo);
    virtual RlcTxEntityBase *lookupRlcTxBuffer(DrbKey id);
    virtual PdcpTxEntityBase *lookupPdcpTxEntity(DrbKey id);
    virtual cModule *lookupPdcpEntityModule(DrbKey id);    // the per-bearer PdcpEntityBase compound (DcMux leg dispatch)
    virtual cModule *lookupPdcpRelayEntityModule(DrbKey id); // the per-bearer PdcpRelayEntity compound (DcMux DL dispatch)
    virtual void deleteLocalPdcpEntities(MacNodeId nodeId);
    virtual void deleteLocalRlcQueues(MacNodeId nodeId, bool nrStack=false);
    virtual void pdcpActiveUeUL(std::set<MacNodeId> *ueSet);
};

} // namespace simu5g

#endif
