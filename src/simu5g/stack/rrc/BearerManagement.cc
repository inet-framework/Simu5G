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

#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/rrc/D2DModeController.h"
#include "simu5g/stack/rrc/Registration.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rlc/um/UmTxEntity.h"
#include "simu5g/stack/pdcp/UpperMux.h"
#include "simu5g/stack/pdcp/DcMux.h"
#include "simu5g/stack/pdcp/PdcpTxEntityBase.h"
#include "simu5g/stack/pdcp/PdcpRxEntityBase.h"
#include "simu5g/stack/pdcp/NrTxPdcpEntity.h"
#include "simu5g/common/InitStages.h"

namespace simu5g {

using namespace inet;

Define_Module(BearerManagement);

void BearerManagement::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        registration_ = inet::getModuleFromPar<Registration>(par("registrationModule"), this);

        // Resolve PDCP entity types
        pdcpEntityModuleType_ = cModuleType::get(par("pdcpEntityModuleType").stringValue());
        pdcpBypassTxEntityModuleType_ = cModuleType::get(par("pdcpBypassTxEntityModuleType").stringValue());
        pdcpBypassRxEntityModuleType_ = cModuleType::get(par("pdcpBypassRxEntityModuleType").stringValue());

        // Resolve RLC entity types (compound modules packaging the TX and RX sides)
        rlcTmEntityModuleType_ = cModuleType::get(par("rlcTmEntityModuleType").stringValue());
        rlcUmEntityModuleType_ = cModuleType::get(par("rlcUmEntityModuleType").stringValue());
        rlcAmEntityModuleType_ = cModuleType::get(par("rlcAmEntityModuleType").stringValue());

        nicModule_ = inet::getContainingNicModule(this);

        binderModule.reference(this, "binderModule", true);
        rlcMuxModule.reference(this, "rlcMuxModule", true);
        nrRlcMuxModule.reference(this, "nrRlcMuxModule", false);
        macModule.reference(this, "macModule", true);
        nrMacModule.reference(this, "nrMacModule", false);
    }
}

void BearerManagement::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void BearerManagement::createIncomingConnection(FlowControlInfo *lteInfo, bool withPdcp)
{
    Enter_Method_Silent("createIncomingConnection()");

    EV << "BearerManagement::createIncomingConnection - " << " srcId=" << lteInfo->getSourceId() << " destId=" << lteInfo->getDestId()
        << " groupId=" << lteInfo->getMulticastGroupId() << " drbId=" << lteInfo->getDrbId()
        << " direction=" << dirToA(lteInfo->getDirection())
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(lteInfo->getDestId() == registration_->getLteNodeId() || lteInfo->getDestId() == registration_->getNrNodeId() || lteInfo->getMulticastGroupId() != NODEID_NONE);

    // Idempotence guard: with duplex bearer establishment this half may already
    // exist (e.g. re-establishment after a partial teardown); skip instead of
    // crashing on duplicate MAC/RLC/PDCP creation.
    DrbKey rlcId = ctrlInfoToRxDrbKey(lteInfo);
    bool isNr = (registration_->getNodeType()==UE && isNrUe(lteInfo->getDestId())); //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    cModule *existingRlcEnt = lookupRlcEntityModule(rlcId, isNr);
    if (existingRlcEnt != nullptr && existingRlcEnt->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::createIncomingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    // Create MAC incoming connection
    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
    MacNodeId senderId = desc.getSourceId();
    auto mac = (registration_->getNodeType()==UE && isNrUe(lteInfo->getDestId())) ? nrMacModule.get() : macModule.get(); //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    LogicalCid lcid = mac->drbIdToLcid(desc.getDrbId());
    MacCid cid = MacCid(senderId, lcid);
    mac->createIncomingConnection(cid, desc);

    // RLC entity creation
    auto *rlcMux = isNr ? nrRlcMuxModule.get() : rlcMuxModule.get();
    installRlcRxSide(rlcId, lteInfo, rlcMux, isNr);

    // PDCP entity creation (compound: TX+RX, see PdcpEntity). The DC-secondary bypass RX
    // stays a flat module.
    if (withPdcp) {
        DrbKey id = DrbKey(lteInfo->getSourceId(), lteInfo->getDrbId());
        installPdcpRxSide(id, rlcMux, isNr);
    }
    else {
        // DC secondary node: create bypass RX entity (forwards UL to master via X2)
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // bypass entities are eNB-only
        DrbKey id = DrbKey(lteInfo->getSourceId(), lteInfo->getDrbId());
        std::string name = "pdcp-bypass-rx-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
        auto *module = pdcpBypassRxEntityModuleType_->create(name.c_str(), nicModule_);
        module->finalizeParameters();
        module->buildInside();
        setEntityDisplayPosition(module, true, rlcMux, num(id.getDrbId()));

        // Wire RLC entity upperOut → bypass PDCP RX in (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, isNr);
        ASSERT(rlcEnt2 != nullptr);
        rlcEnt2->gate("upperOut")->connectTo(module->gate("in"));

        // Wire entity out gate → DcMux (bypass RX sends to X2 via DcMux)
        int fromIdx = pdcpDcMux->gateSize("fromEntity");
        pdcpDcMux->setGateSize("fromEntity", fromIdx + 1);
        module->gate("out")->connectTo(pdcpDcMux->gate("fromEntity", fromIdx));

        module->scheduleStart(simTime());
        module->callInitialize();
        auto *rxEnt = check_and_cast<PdcpRxEntityBase *>(module);
        pdcpBypassRxEntities_[id] = rxEnt;
    }
}

void BearerManagement::createOutgoingConnection(FlowControlInfo *lteInfo, bool withPdcp)
{
    Enter_Method_Silent("createOutgoingConnection()");

    EV << "BearerManagement::createOutgoingConnection - " << " srcId=" << lteInfo->getSourceId() << " destId=" << lteInfo->getDestId()
        << " groupId=" << lteInfo->getMulticastGroupId() << " drbId=" << lteInfo->getDrbId()
        << " direction=" << dirToA(lteInfo->getDirection())
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(lteInfo->getSourceId() == registration_->getLteNodeId() || lteInfo->getSourceId() == registration_->getNrNodeId());

    // Idempotence guard: with duplex bearer establishment this half may already
    // exist (e.g. re-establishment after a partial teardown); skip instead of
    // crashing on duplicate MAC/RLC/PDCP creation.
    DrbKey rlcId = ctrlInfoToTxDrbKey(lteInfo);
    bool isNr = (registration_->getNodeType()==UE && isNrUe(lteInfo->getSourceId()));
    cModule *existingRlcEnt = lookupRlcEntityModule(rlcId, isNr);
    if (existingRlcEnt != nullptr && existingRlcEnt->gate("lowerOut")->isConnectedOutside()) {
        EV << "BearerManagement::createOutgoingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    // Create MAC outgoing connection
    FlowDescriptor desc = FlowDescriptor::fromFlowControlInfo(*lteInfo);
    MacNodeId destId = desc.getDestId();
    auto mac = (registration_->getNodeType()==UE && isNrUe(lteInfo->getSourceId())) ? nrMacModule.get() : macModule.get();
    LogicalCid lcid = mac->drbIdToLcid(desc.getDrbId());
    MacCid cid = MacCid(destId, lcid);
    mac->createOutgoingConnection(cid, desc);

    // RLC entity creation
    auto *rlcMux = isNr ? nrRlcMuxModule.get() : rlcMuxModule.get();
    installRlcTxSide(rlcId, lteInfo, rlcMux, isNr);

    // PDCP entity creation (compound: TX+RX, see PdcpEntity). The DC-secondary bypass TX
    // stays a flat module.
    if (withPdcp) {
        DrbKey id = DrbKey(lteInfo->getDestId(), lteInfo->getDrbId());
        installPdcpTxSide(id, lteInfo, rlcMux, isNr);
    }
    else {
        // DC secondary node: create bypass TX entity (forwards DL from master to RLC)
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // bypass entities are eNB-only
        DrbKey id = DrbKey(lteInfo->getDestId(), lteInfo->getDrbId());
        std::string name = "pdcp-bypass-tx-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
        auto *module = pdcpBypassTxEntityModuleType_->create(name.c_str(), nicModule_);
        module->finalizeParameters();
        module->buildInside();
        setEntityDisplayPosition(module, true, rlcMux, num(id.getDrbId()));

        // Wire DcMux → entity in gate (DcMux dispatches incoming DL X2)
        int idx = pdcpDcMux->gateSize("toBypassTxEntity");
        pdcpDcMux->setGateSize("toBypassTxEntity", idx + 1);
        pdcpDcMux->gate("toBypassTxEntity", idx)->connectTo(module->gate("in"));

        // Wire bypass TX out → RLC entity upperIn (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, isNr);
        ASSERT(rlcEnt2 != nullptr);
        module->gate("out")->connectTo(rlcEnt2->gate("upperIn"));

        module->scheduleStart(simTime());
        module->callInitialize();
        auto *txEnt = check_and_cast<PdcpTxEntityBase *>(module);
        pdcpDcMux->registerBypassTxEntity(id, txEnt);
        pdcpBypassTxEntities_[id] = txEnt;
    }
}

void BearerManagement::setRlcEntityParams(cModule *entity, bool isNr)
{
    if (entity->hasPar("macModule"))  // entities sit inside the compound: NIC is two levels up
        entity->par("macModule").setStringValue(isNr ? "^.^.nrMac" : "^.^.mac");
    if (entity->hasPar("isNR"))
        entity->par("isNR").setBoolValue(isNr);
}

void BearerManagement::setEntityDisplayPosition(cModule *entity, bool isPdcpEntity, cModule *rlcMux, int bearerIndex)
{
    auto *pdcpMux = inet::getModuleFromPar<UpperMux>(par("pdcpMuxModule"), this);
    if (!pdcpMux || !rlcMux)
        return;

    int uy = atoi(pdcpMux->getDisplayString().getTagArg("p", 1));
    int lx = atoi(rlcMux->getDisplayString().getTagArg("p", 0));
    int ly = atoi(rlcMux->getDisplayString().getTagArg("p", 1));

    int x = lx + 60 * bearerIndex;
    int y = isPdcpEntity ? uy + (ly - uy) / 3 : uy + 2 * (ly - uy) / 3;

    entity->getDisplayString().setTagArg("p", 0, x);
    entity->getDisplayString().setTagArg("p", 1, y);
}

cModule *BearerManagement::lookupRlcEntityModule(DrbKey id, bool isNr)
{
    auto& entities = isNr ? nrRlcEntities_ : rlcEntities_;
    auto it = entities.find(id);
    return it != entities.end() ? it->second : nullptr;
}

cModule *BearerManagement::findOrCreateRlcEntity(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    auto& entities = isNr ? nrRlcEntities_ : rlcEntities_;
    auto it = entities.find(id);
    if (it != entities.end())
        return it->second;

    // Create the per-bearer RLC entity module (compound: TX + RX sides). With duplex
    // bearer establishment the first-processed direction creates it; the other
    // direction finds it here and just installs (wires) its own side.
    LteRlcType rlcType = static_cast<LteRlcType>(lteInfo->getRlcType());
    cModuleType *moduleType;
    const char *prefix;
    switch (rlcType) {
        case TM: moduleType = rlcTmEntityModuleType_; prefix = "tm"; break;
        case AM: moduleType = rlcAmEntityModuleType_; prefix = "am"; break;
        default: moduleType = rlcUmEntityModuleType_; prefix = "um"; break;
    }
    std::string name = std::string(isNr ? "nrRlc-" : "rlc-") + prefix + "-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = moduleType->create(name.c_str(), nicModule_);
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, false, rlcMux, num(id.getDrbId()));

    // Parametrize both sides (params remain settable until the entities initialize)
    setRlcEntityParams(module->getSubmodule("tx"), isNr);
    setRlcEntityParams(module->getSubmodule("rx"), isNr);

    module->scheduleStart(simTime());
    module->callInitialize();

    entities[id] = module;
    return module;
}

RlcTxEntityBase *BearerManagement::installRlcTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    cModule *module = findOrCreateRlcEntity(id, lteInfo, rlcMux, isNr);
    auto *txEnt = check_and_cast<RlcTxEntityBase *>(module->getSubmodule("tx"));

    if (module->gate("lowerOut")->isConnectedOutside()) {
        EV << "BearerManagement::installRlcTxSide - TX side of " << id.str() << " already installed\n";
        return txEnt;
    }

    // Wire entity lowerOut gate → LowerMux fromTxEntity
    int fromIdx = rlcMux->gateSize("fromTxEntity");
    rlcMux->setGateSize("fromTxEntity", fromIdx + 1);
    module->gate("lowerOut")->connectTo(rlcMux->gate("fromTxEntity", fromIdx));

    // Wire LowerMux macToTxEntity → entity macIn gate
    int macIdx = rlcMux->gateSize("macToTxEntity");
    rlcMux->setGateSize("macToTxEntity", macIdx + 1);
    rlcMux->gate("macToTxEntity", macIdx)->connectTo(module->gate("macIn"));

    txEnt->setFlowControlInfo(lteInfo);

    // D2D peer tracking (only for UM TX entities)
    if (static_cast<LteRlcType>(lteInfo->getRlcType()) == UM) {
        auto *d2dCtrl = inet::findModuleFromPar<D2DModeController>(par("d2dModeControllerModule"), this);
        if (d2dCtrl) {
            auto *umTxEnt = check_and_cast<UmTxEntity *>(txEnt);
            d2dCtrl->registerD2DPeerTxEntity(MacNodeId(lteInfo->getD2dRxPeerId()), umTxEnt);
        }
    }

    return txEnt;
}

RlcRxEntityBase *BearerManagement::installRlcRxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    cModule *module = findOrCreateRlcEntity(id, lteInfo, rlcMux, isNr);
    auto *rxEnt = check_and_cast<RlcRxEntityBase *>(module->getSubmodule("rx"));

    if (module->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::installRlcRxSide - RX side of " << id.str() << " already installed\n";
        return rxEnt;
    }

    // Wire LowerMux → entity lowerIn gate
    int idx = rlcMux->gateSize("toRxEntity");
    rlcMux->setGateSize("toRxEntity", idx + 1);
    rlcMux->gate("toRxEntity", idx)->connectTo(module->gate("lowerIn"));

    rxEnt->setFlowControlInfo(lteInfo);

    // Register in mux routing table
    rlcMux->registerRxBuffer(id, rxEnt);

    return rxEnt;
}

cModule *BearerManagement::findOrCreatePdcpEntity(DrbKey id, RlcMux *rlcMux)
{
    auto it = pdcpEntities_.find(id);
    if (it != pdcpEntities_.end())
        return it->second;

    // Create the per-bearer PDCP entity module (compound: TX + RX sides). With duplex bearer
    // establishment the first-processed direction creates it; the other finds it here and just
    // installs (wires) its own side.
    std::string name = "pdcp-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = pdcpEntityModuleType_->create(name.c_str(), nicModule_);
    module->par("headerCompressedSize") = par("headerCompressedSize");
    // TX/RX entity profiles are set independently (an EN-DC master mixes NrTx with LteRx)
    module->par("txEntityType") = par("pdcpTxEntityModuleType").stringValue();
    module->par("rxEntityType") = par("pdcpRxEntityModuleType").stringValue();
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, true, rlcMux, num(id.getDrbId()));
    module->scheduleStart(simTime());
    module->callInitialize();

    pdcpEntities_[id] = module;
    return module;
}

void BearerManagement::installPdcpTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    auto *pdcpMux = inet::getModuleFromPar<UpperMux>(par("pdcpMuxModule"), this);
    auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this); // nullptr on UEs (no X2)
    // The PDCP entity is keyed by dest (id); the RLC entity it wires to is keyed by
    // ctrlInfoToTxDrbKey, which for multicast is the group id -- not the same as id.
    DrbKey rlcId = ctrlInfoToTxDrbKey(lteInfo);

    // DC UE NR leg: reuse the master (LTE-leg) PDCP entity's nrOut gate instead of creating a
    // new PDCP entity. The master leg's entity of the SAME bearer is keyed by (master nodeB,
    // same DRB id) -- look it up precisely (matching the bare DRB id would also match unrelated
    // bearers of this UE, since DRB ids are only unique per peer).
    if (registration_->getNodeType()==UE && isNrUe(lteInfo->getSourceId())
            && getNodeTypeById(lteInfo->getDestId()) == NODEB) {
        MacNodeId masterNodeId = binderModule->getMasterNodeOrSelf(lteInfo->getDestId());
        if (masterNodeId != lteInfo->getDestId()) {  // dest is a DC secondary node
            auto masterIt = pdcpEntities_.find(DrbKey(masterNodeId, id.getDrbId()));
            if (masterIt != pdcpEntities_.end()) {
                cModule *nrRlcEnt = lookupRlcEntityModule(rlcId, true);
                ASSERT(nrRlcEnt != nullptr);
                masterIt->second->gate("nrOut")->connectTo(nrRlcEnt->gate("upperIn"));
                return;  // wired to the master entity; no PDCP TX entity for the NR leg
            }
        }
    }

    cModule *pdcpEnt = findOrCreatePdcpEntity(id, rlcMux);
    if (pdcpEnt->gate("lowerOut")->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpTxSide - TX side of " << id.str() << " already installed\n";
        return;
    }

    // Wire UpperMux → compound upperIn (→ tx.in)
    int idx = pdcpMux->gateSize("toTxEntity");
    pdcpMux->setGateSize("toTxEntity", idx + 1);
    pdcpMux->gate("toTxEntity", idx)->connectTo(pdcpEnt->gate("upperIn"));

    // Wire compound lowerOut (← tx.out) → RLC entity upperIn (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, isNr);
    ASSERT(rlcEnt != nullptr);
    pdcpEnt->gate("lowerOut")->connectTo(rlcEnt->gate("upperIn"));

    // Wire compound dcOut (← tx.dcOut) → DcMux, for an NR-typed TX on nodes with a DcMux (eNB).
    // The uniform compound/LteTx always has a dcOut gate now, so discriminate on the TX entity
    // type (only NrTxPdcpEntity actually drives dcOut) -- the faithful successor to the old
    // per-entity hasGate("dcOut") check.
    if (pdcpDcMux && dynamic_cast<NrTxPdcpEntity *>(pdcpEnt->getSubmodule("tx")) != nullptr) {
        int dcIdx = pdcpDcMux->gateSize("fromEntity");
        pdcpDcMux->setGateSize("fromEntity", dcIdx + 1);
        pdcpEnt->gate("dcOut")->connectTo(pdcpDcMux->gate("fromEntity", dcIdx));
    }

    auto *txEnt = check_and_cast<PdcpTxEntityBase *>(pdcpEnt->getSubmodule("tx"));
    pdcpMux->registerTxEntity(id, txEnt);
    pdcpTxEntities_[id] = txEnt;
}

void BearerManagement::installPdcpRxSide(DrbKey id, RlcMux *rlcMux, bool isNr)
{
    auto *pdcpMux = inet::getModuleFromPar<UpperMux>(par("pdcpMuxModule"), this);
    auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this); // nullptr on UEs (no X2)

    cModule *pdcpEnt = findOrCreatePdcpEntity(id, rlcMux);
    if (pdcpEnt->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpRxSide - RX side of " << id.str() << " already installed\n";
        return;
    }

    // Wire RLC entity upperOut → compound lowerIn (→ rx.in) (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(id, isNr);
    ASSERT(rlcEnt != nullptr);
    rlcEnt->gate("upperOut")->connectTo(pdcpEnt->gate("lowerIn"));

    // Wire compound upperOut (← rx.out) → UpperMux fromRxEntity
    int fromIdx = pdcpMux->gateSize("fromRxEntity");
    pdcpMux->setGateSize("fromRxEntity", fromIdx + 1);
    pdcpEnt->gate("upperOut")->connectTo(pdcpMux->gate("fromRxEntity", fromIdx));

    // Wire DcMux → compound dcIn (→ rx.dcIn) for UL X2 dispatch (nodes with a DcMux, i.e. eNB).
    // Both LTE and NR RX entities have dcIn, so this follows only the DcMux presence, as before.
    if (pdcpDcMux) {
        int dcIdx = pdcpDcMux->gateSize("toRxEntity");
        pdcpDcMux->setGateSize("toRxEntity", dcIdx + 1);
        pdcpDcMux->gate("toRxEntity", dcIdx)->connectTo(pdcpEnt->gate("dcIn"));
    }

    auto *rxEnt = check_and_cast<PdcpRxEntityBase *>(pdcpEnt->getSubmodule("rx"));
    pdcpRxEntities_[id] = rxEnt;
}

RlcTxEntityBase *BearerManagement::createRlcTxBuffer(DrbKey id, FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createRlcTxBuffer()");
    return installRlcTxSide(id, lteInfo, rlcMuxModule.get(), false);
}

RlcRxEntityBase *BearerManagement::createRlcRxBuffer(DrbKey id, FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createRlcRxBuffer()");
    return installRlcRxSide(id, lteInfo, rlcMuxModule.get(), false);
}

void BearerManagement::deleteLocalPdcpEntities(MacNodeId nodeId)
{
    Enter_Method_Silent("deleteLocalPdcpEntities()");

    auto *pdcpMux = inet::getModuleFromPar<UpperMux>(par("pdcpMuxModule"), this);
    auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this); // nullptr on UEs (no X2)

    bool isEnb = (registration_->getNodeType() == NODEB);

    // Per-node (keyed) deletion at eNBs/gNBs and at NR-capable UEs; wipe-all only at plain
    // LTE UEs. This mirrors the pre-flattening per-class behavior (LtePdcpEnb and NrPdcpUe
    // deleted entities keyed by node, LtePdcpUe deleted all). At an NR UE this single module
    // holds the PDCP entities of BOTH legs (keyed by the peer node): a one-leg detach must
    // not delete the other leg's entities, otherwise that leg's RLC RX entities are left
    // forwarding to a dangling gate, and later re-establishment collides with its leftovers.
    bool keyed = isEnb || registration_->getNrNodeId() != NODEID_NONE;

    // Delete full PDCP entity compounds (each deletes its TX and RX side). Unregister the TX
    // from the UpperMux routing table where one was installed -- a DC NR leg reuses the master's
    // TX via nrOut, so its compound carries only an (idle) TX submodule, never registered.
    for (auto it = pdcpEntities_.begin(); it != pdcpEntities_.end(); ) {
        if (!keyed || it->first.getNodeId() == nodeId) {
            if (pdcpTxEntities_.count(it->first)) {
                pdcpMux->unregisterTxEntity(it->first);
                pdcpTxEntities_.erase(it->first);
            }
            pdcpRxEntities_.erase(it->first);
            it->second->deleteModule();
            it = pdcpEntities_.erase(it);
        } else ++it;
    }

    // Delete bypass TX entities (eNB-only)
    ASSERT(pdcpBypassTxEntities_.empty() || pdcpDcMux != nullptr);
    for (auto it = pdcpBypassTxEntities_.begin(); it != pdcpBypassTxEntities_.end(); ) {
        if (!keyed || it->first.getNodeId() == nodeId) {
            pdcpDcMux->unregisterBypassTxEntity(it->first);
            it->second->deleteModule();
            it = pdcpBypassTxEntities_.erase(it);
        } else ++it;
    }

    // Delete bypass RX entities
    for (auto it = pdcpBypassRxEntities_.begin(); it != pdcpBypassRxEntities_.end(); ) {
        if (!keyed || it->first.getNodeId() == nodeId) {
            it->second->deleteModule();
            it = pdcpBypassRxEntities_.erase(it);
        } else ++it;
    }
}

void BearerManagement::deleteLocalRlcQueues(MacNodeId nodeId, bool nrStack)
{
    Enter_Method_Silent("deleteLocalRlcQueues()");

    bool isEnb = (registration_->getNodeType() == NODEB);

    // At a NODEB, entities are always stored in the default (LTE) maps regardless of which
    // leg the caller serves: createIncoming/OutgoingConnection() computes isNr as
    // (nodeType==UE && ...), which is always false here, and gNB NICs have no nrRlcMux.
    // Honoring the caller's nrStack flag would make this a silent no-op, leaking the UE's
    // entities and crashing on re-establishment after a later handover.
    if (isEnb)
        nrStack = false;

    auto &entities = nrStack ? nrRlcEntities_ : rlcEntities_;
    RlcMux *rlcMux = nrStack ? (nrRlcMuxModule ? nrRlcMuxModule.get() : nullptr) : rlcMuxModule.get();
    if (!rlcMux)
        return;

    // Delete the per-bearer RLC entity modules (each deletes its TX and RX side)
    for (auto it = entities.begin(); it != entities.end(); ) {
        if (isEnb ? it->first.getNodeId() == nodeId : true) {
            rlcMux->unregisterRxBuffer(it->first);  // no-op if the RX side was never installed
            it->second->deleteModule();
            it = entities.erase(it);
        } else ++it;
    }
}

PdcpRxEntityBase *BearerManagement::lookupPdcpRxEntity(DrbKey id)
{
    auto it = pdcpRxEntities_.find(id);
    if (it != pdcpRxEntities_.end())
        return it->second;
    auto it2 = pdcpBypassRxEntities_.find(id);
    return it2 != pdcpBypassRxEntities_.end() ? it2->second : nullptr;
}

RlcTxEntityBase *BearerManagement::lookupRlcTxBuffer(DrbKey id)
{
    // Search both legs. Only an INSTALLED (mux-wired) TX side counts: with the
    // compound entity, the RX-side install may have created the module while the
    // TX side is not set up yet -- callers create it via createRlcTxBuffer then.
    for (auto *entities : { &rlcEntities_, &nrRlcEntities_ }) {
        auto it = entities->find(id);
        if (it != entities->end() && it->second->gate("lowerOut")->isConnectedOutside())
            return check_and_cast<RlcTxEntityBase *>(it->second->getSubmodule("tx"));
    }
    return nullptr;
}

PdcpTxEntityBase *BearerManagement::lookupPdcpTxEntity(DrbKey id)
{
    auto it = pdcpTxEntities_.find(id);
    return it != pdcpTxEntities_.end() ? it->second : nullptr;
}

void BearerManagement::pdcpActiveUeUL(std::set<MacNodeId> *ueSet)
{
    for (const auto& [id, rxEntity] : pdcpRxEntities_) {
        if (!rxEntity->isEmpty())
            ueSet->insert(id.getNodeId());
    }
}

} // namespace simu5g
