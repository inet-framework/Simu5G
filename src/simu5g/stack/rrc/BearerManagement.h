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

using namespace omnetpp;

namespace simu5g {

class LteMacBase;
class RlcMux;
class RlcTxEntityBase;
class RlcRxEntityBase;
class UmTxEntity;
class UpperMux;
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
  private:
    Registration *registration_ = nullptr;

    // PDCP entity types (resolved from NED params): the full PDCP entity is a compound
    // (PdcpEntity/NrPdcpEntity, TX+RX); the DC-secondary bypass entities are flat.
    cModuleType *pdcpEntityModuleType_ = nullptr;
    cModuleType *pdcpBypassRxEntityModuleType_ = nullptr;
    cModuleType *pdcpBypassTxEntityModuleType_ = nullptr;
    cModule *nicModule_ = nullptr;  // containing NIC module (parent of all submodules and entities)

    // RLC entity types (resolved from NED params)
    cModuleType *rlcTmEntityModuleType_ = nullptr;
    cModuleType *rlcUmEntityModuleType_ = nullptr;
    cModuleType *rlcAmEntityModuleType_ = nullptr;

    inet::ModuleRefByPar<RlcMux> rlcMuxModule;
    inet::ModuleRefByPar<RlcMux> nrRlcMuxModule;
    inet::ModuleRefByPar<LteMacBase> macModule;
    inet::ModuleRefByPar<LteMacBase> nrMacModule;
    inet::ModuleRefByPar<Binder> binderModule;   // for DC master/secondary topology lookups

    // Entity registries (CP owns the lifecycle of all entities)
    // One PDCP entity module (compound: TX+RX, see PdcpEntity) per bearer, keyed by (peer
    // node, DRB id); the tx/rx submodule pointers below index into it for per-side lookups.
    std::map<DrbKey, cModule *> pdcpEntities_;
    std::map<DrbKey, PdcpTxEntityBase *> pdcpTxEntities_;
    std::map<DrbKey, PdcpRxEntityBase *> pdcpRxEntities_;
    std::map<DrbKey, PdcpTxEntityBase *> pdcpBypassTxEntities_;
    std::map<DrbKey, PdcpRxEntityBase *> pdcpBypassRxEntities_;
    // One RLC entity module (compound: TX+RX sides, see RlcTm/Um/AmEntity) per
    // bearer, keyed by (peer node, DRB id); one map per leg (LTE / NR)
    std::map<DrbKey, cModule *> rlcEntities_;
    std::map<DrbKey, cModule *> nrRlcEntities_;

    void setRlcEntityParams(cModule *entity, bool isNr);
    void setEntityDisplayPosition(cModule *entity, bool isPdcpEntity, cModule *rlcMux, int bearerIndex);
    cModule *lookupRlcEntityModule(DrbKey id, bool isNr);
    cModule *findOrCreateRlcEntity(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    RlcTxEntityBase *installRlcTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    RlcRxEntityBase *installRlcRxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    cModule *findOrCreatePdcpEntity(DrbKey id, RlcMux *rlcMux);
    void installPdcpTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr);
    void installPdcpRxSide(DrbKey id, RlcMux *rlcMux, bool isNr);

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

  public:
    virtual void createIncomingConnection(FlowControlInfo *lteInfo, bool withPdcp=true);
    virtual void createOutgoingConnection(FlowControlInfo *lteInfo, bool withPdcp=true);
    virtual RlcTxEntityBase *createRlcTxBuffer(DrbKey id, FlowControlInfo *lteInfo);
    virtual RlcRxEntityBase *createRlcRxBuffer(DrbKey id, FlowControlInfo *lteInfo);
    RlcTxEntityBase *lookupRlcTxBuffer(DrbKey id);
    PdcpTxEntityBase *lookupPdcpTxEntity(DrbKey id);
    PdcpRxEntityBase *lookupPdcpRxEntity(DrbKey id);
    virtual void deleteLocalPdcpEntities(MacNodeId nodeId);
    virtual void deleteLocalRlcQueues(MacNodeId nodeId, bool nrStack=false);
    void pdcpActiveUeUL(std::set<MacNodeId> *ueSet);
};

} // namespace simu5g

#endif
