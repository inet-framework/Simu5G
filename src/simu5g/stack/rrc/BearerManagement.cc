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
#include "simu5g/stack/ip2nic/Ip2Nic.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/um/RlcUmTxEntityBase.h"
#include "simu5g/stack/pdcp/PdcpMux.h"
#include "simu5g/stack/pdcp/DcMux.h"
#include "simu5g/stack/pdcp/PdcpTxEntityBase.h"
#include "simu5g/stack/pdcp/PdcpRxEntityBase.h"
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
        pdcpRelayEntityModuleType_ = cModuleType::get(par("pdcpRelayEntityModuleType").stringValue());

        // Resolve RLC entity types (compound modules packaging the TX and RX sides);
        // lteRlc* serve LTE-FI bearers, nrRlc* serve NR (SI/SO) bearers
        rlcTmEntityModuleType_ = cModuleType::get(par("rlcTmEntityModuleType").stringValue());
        lteRlcUmEntityModuleType_ = cModuleType::get(par("lteRlcUmEntityModuleType").stringValue());
        lteRlcAmEntityModuleType_ = cModuleType::get(par("lteRlcAmEntityModuleType").stringValue());
        nrRlcUmEntityModuleType_ = cModuleType::get(par("nrRlcUmEntityModuleType").stringValue());
        nrRlcAmEntityModuleType_ = cModuleType::get(par("nrRlcAmEntityModuleType").stringValue());

        nicModule_ = inet::getContainingNicModule(this);

        binderModule.reference(this, "binderModule", true);
        rlcMuxModule.reference(this, "rlcMuxModule", true);
        nrRlcMuxModule.reference(this, "nrRlcMuxModule", false);
        macModule.reference(this, "macModule", true);
        nrMacModule.reference(this, "nrMacModule", false);

        t311_ = par("t311");
        t301_ = par("t301");

        dualConnectivityEnabled_ = par("dualConnectivityEnabled");
    }
}

BearerManagement::~BearerManagement()
{
    cancelAndDelete(rlfTrigger_);
    for (auto& [msg, peerId] : t311Timers_)
        cancelAndDelete(msg);
    for (auto& [msg, peerId] : t301Timers_)
        cancelAndDelete(msg);
}

void BearerManagement::handleMessage(cMessage *msg)
{
    if (msg == rlfTrigger_) {
        // Drain deferred radio-link-failure teardowns at a safe point (a fresh event),
        // never from inside RLC/PDCP processing.
        auto pending = pendingRlf_;   // copy: teardown deletes entity modules
        pendingRlf_.clear();
        for (const auto &[nodeId, nrStack] : pending)
            handleRadioLinkFailure(nodeId, nrStack);
        return;
    }
    // RRC re-establishment (TS 38.331 5.3.7), two phases; see releaseLink().
    auto t1 = t311Timers_.find(msg);
    if (t1 != t311Timers_.end()) {
        // T311 expiry: a suitable cell is assumed selected (single-cell). Send the
        // RRCReestablishmentRequest and start T301 (request -> complete).
        MacNodeId peerId = t1->second;
        t311Timers_.erase(t1);
        EV << NOW << " BearerManagement - RRC re-establishment: T311 done for node " << peerId
           << ", suitable cell selected; RRCReestablishmentRequest sent, T301 started ("
           << t301_ << "s)" << endl;
        cMessage *t301 = new cMessage("rrcT301");
        t301Timers_[t301] = peerId;
        scheduleAfter(t301_, t301);
        delete msg;
        return;
    }
    auto t3 = t301Timers_.find(msg);
    if (t3 != t301Timers_.end()) {
        // T301 expiry: RRCReestablishmentComplete -- un-release the peer so its bearer
        // re-establishes on demand and DL/UL traffic resumes.
        MacNodeId peerId = t3->second;
        t301Timers_.erase(t3);
        if (auto *ip2nic = inet::findModuleFromPar<Ip2Nic>(par("ip2nicModule"), this))
            ip2nic->resumeUe(peerId);
        EV << NOW << " BearerManagement - RRC re-establishment complete for node " << peerId
           << "; bearer re-establishes on demand, traffic resumes" << endl;
        delete msg;
        return;
    }
    throw cRuntimeError("This module does not process messages");
}

void BearerManagement::scheduleRadioLinkFailure(MacNodeId nodeId, bool nrStack)
{
    Enter_Method_Silent("scheduleRadioLinkFailure()");
    EV << NOW << " BearerManagement::scheduleRadioLinkFailure - node " << nodeId
       << (nrStack ? " (NR)" : " (LTE)") << endl;
    pendingRlf_.insert({nodeId, nrStack});
    if (!rlfTrigger_)
        rlfTrigger_ = new cMessage("rlfTrigger");
    if (!rlfTrigger_->isScheduled())
        scheduleAt(simTime(), rlfTrigger_);
}

void BearerManagement::handleRadioLinkFailure(MacNodeId nodeId, bool nrStack)
{
    EV << NOW << " BearerManagement::handleRadioLinkFailure - RLF for node " << nodeId
       << (nrStack ? " (NR)" : " (LTE)") << endl;
    // Release + tear down the link on BOTH ends, so if RRC re-establishment is enabled the
    // bearer rebuilds fresh, SN-consistent entities on both sides. Reaching the peer via the
    // binder mirrors handover's cross-node HandoverController::deleteOldBuffers.
    releaseLink(nodeId);
    // Address the peer's symmetric teardown with OUR node id on the failing leg: the peer keys
    // its entities (PDCP/RLC/MAC) for this link by that id (the source/dest id it saw on the
    // bearer). Using getLteNodeId() unconditionally sent NODEID_NONE from a standalone NR gNB
    // (whose id lives in nrNodeId), so the peer's keyed PDCP deletion missed pdcp-rx-<gnb>-<drb>
    // and a later re-establishment collided with the leftover entity.
    MacNodeId myId = nrStack ? registration_->getNrNodeId() : registration_->getLteNodeId();
    if (cModule *peerRrc = binderModule->getRrcByNodeId(nodeId)) {
        if (auto *peerBm = dynamic_cast<BearerManagement *>(peerRrc->getSubmodule("bearerManagement")))
            peerBm->releaseLink(myId);
    }
}

void BearerManagement::releaseLink(MacNodeId peerId)
{
    Enter_Method_Silent("releaseLink()");
    EV << NOW << " BearerManagement::releaseLink - releasing link to " << peerId << endl;
    // UE Context Release: stop traffic at Ip2Nic first, so no packet is pushed at a torn-down
    // bearer (which would otherwise hit PdcpMux's TX-entity assert).
    if (auto *ip2nic = inet::findModuleFromPar<Ip2Nic>(par("ip2nicModule"), this)) {
        ip2nic->releaseUe(peerId);
        // RRC re-establishment (TS 38.331 5.3.7): start the cell-(re)selection timer T311. When it
        // fires a suitable cell is assumed selected and the RRCReestablishmentRequest->Complete
        // (T301) exchange runs, after which the peer is un-released and its bearer re-establishes
        // on demand. t311_ = 0 disables re-establishment (the peer stays released -> RRC_IDLE).
        if (t311_ > SIMTIME_ZERO) {
            cMessage *t311 = new cMessage("rrcT311");
            t311Timers_[t311] = peerId;
            scheduleAfter(t311_, t311);
            EV << NOW << " BearerManagement::releaseLink - RRC re-establishment: T311 (cell selection) started for node "
               << peerId << " (" << t311_ << "s)" << endl;
        }
    }
    // Tear down BOTH legs' MAC/HARQ + RLC (leg-agnostic: a bearer may live on the LTE-FI or
    // NR-SO leg, and on a gNB every entity is on the base leg regardless of the UE being NR),
    // then the shared PDCP entities. On a UE these delete all local entities (not nodeId-
    // filtered on the UE side), which is what RLF -- a link-level event -- requires.
    if (macModule)
        macModule->deleteQueuesRadioLinkFailure(peerId);
    if (nrMacModule)
        nrMacModule->deleteQueuesRadioLinkFailure(peerId);
    deleteLocalRlcQueues(peerId, /*nrStack=*/false);
    deleteLocalRlcQueues(peerId, /*nrStack=*/true);
    deleteLocalPdcpEntities(peerId);
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

    // PDCP entity creation (compound: TX+RX, see PdcpEntityBase). At a DC secondary the bearer's
    // PDCP lives at the master, so an PdcpRelayEntity stands in (UL half wired here).
    if (withPdcp) {
        DrbKey id = DrbKey(lteInfo->getSourceId(), lteInfo->getDrbId());
        installPdcpRxSide(id, lteInfo, rlcMux, isNr);
    }
    else {
        // DC secondary node: forward the UL PDU from RLC to the master over X2 unprocessed
        // (PdcpRelayEntity's UL half, see PdcpRelayEntity).
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // relay entities are eNB-only
        DrbKey id = DrbKey(lteInfo->getSourceId(), lteInfo->getDrbId());
        cModule *relay = findOrCreatePdcpRelayEntity(id, rlcMux);

        // Wire RLC entity upperOut → relay legIn[0] (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, isNr);
        ASSERT(rlcEnt2 != nullptr);
        rlcEnt2->gate("upperOut")->connectTo(relay->gate("legIn", 0));

        // Wire relay x2Out → DcMux (UL PDU tunneled to the master)
        int fromIdx = pdcpDcMux->gateSize("fromEntity");
        pdcpDcMux->setGateSize("fromEntity", fromIdx + 1);
        relay->gate("x2Out")->connectTo(pdcpDcMux->gate("fromEntity", fromIdx));
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

    // PDCP entity creation (compound: TX+RX, see PdcpEntityBase). At a DC secondary the bearer's
    // PDCP lives at the master, so an PdcpRelayEntity stands in (DL half wired here).
    if (withPdcp) {
        DrbKey id = DrbKey(lteInfo->getDestId(), lteInfo->getDrbId());
        installPdcpTxSide(id, lteInfo, rlcMux, isNr);
    }
    else {
        // DC secondary node: the DL PDU arrives already PDCP-processed from the master over X2;
        // forward it straight to RLC (PdcpRelayEntity's DL half, see PdcpRelayEntity).
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // relay entities are eNB-only
        DrbKey id = DrbKey(lteInfo->getDestId(), lteInfo->getDrbId());
        cModule *relay = findOrCreatePdcpRelayEntity(id, rlcMux);

        // Wire DcMux → relay x2In (DcMux dispatches the incoming DL X2 PDU)
        int idx = pdcpDcMux->gateSize("toRelayEntity");
        pdcpDcMux->setGateSize("toRelayEntity", idx + 1);
        pdcpDcMux->gate("toRelayEntity", idx)->connectTo(relay->gate("x2In"));

        // Wire relay legOut[0] → RLC entity upperIn (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, isNr);
        ASSERT(rlcEnt2 != nullptr);
        relay->gate("legOut", 0)->connectTo(rlcEnt2->gate("upperIn"));
    }
}

void BearerManagement::setRlcEntityParams(cModule *entity, bool isNr)
{
    // 'entity' is the RlcTm/Um/AmEntity COMPOUND; these paths are relative to it (^ = the NIC).
    // The compound NED absPath()-resolves them and passes them to its tx/rx submodules (see
    // '*.macModule = default(absPath(this.macModule))'). hasPar() guards because not every
    // compound/leg declares both (TM has neither; AM has no rlcMux). Per leg: a UE's NR leg uses
    // nrMac/nrRlcMux; everything else (incl. a gNB's NR bearers, which have no nrMac) uses mac/rlcMux.
    if (entity->hasPar("macModule"))
        entity->par("macModule").setStringValue(isNr ? "^.nrMac" : "^.mac");
    if (entity->hasPar("rlcMuxModule"))
        entity->par("rlcMuxModule").setStringValue(isNr ? "^.nrRlcMux" : "^.rlcMux");
    // The RLC entity's wire format is set by its own soFraming parameter (via the Nr*
    // NED profile), selected per bearer by the RAT: an NR bearer uses the SO/no-concat
    // framing of TS 38.322, an LTE bearer the FI/concatenation of TS 36.322. The framing
    // is a function of the RAT, not an independent knob; soFraming is a separate
    // parameter only because it is the selection mechanism. (The NR-leg marker proper is
    // the pdcpMux.isNR parameter, read by PdcpMux/Ip2Nic/LteMacEnb, not set here.)
}

void BearerManagement::setEntityDisplayPosition(cModule *entity, bool isPdcpEntity, cModule *rlcMux, int bearerIndex)
{
    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
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
    // The entity TYPE (LTE-FI vs NR-SO wire format) must be identical at both ends of
    // the bearer, so it keys on whether the bearer's UE is an NR UE — a symmetric
    // property of the source/dest node ids — not on the local-node isNr flag (which is
    // only ever true at a UE, leaving the gNB on the LTE type and breaking the wire).
    // For a DC UE this still separates the NR-secondary leg from the LTE-master leg,
    // since those bearers reference the UE's NR vs LTE node id respectively.
    bool isNrBearer = isNrUe(lteInfo->getSourceId()) || isNrUe(lteInfo->getDestId());
    cModuleType *moduleType;
    const char *prefix;
    switch (rlcType) {
        case TM: moduleType = rlcTmEntityModuleType_; prefix = "tm"; break;
        case AM: moduleType = isNrBearer ? nrRlcAmEntityModuleType_ : lteRlcAmEntityModuleType_; prefix = "am"; break;
        default: moduleType = isNrBearer ? nrRlcUmEntityModuleType_ : lteRlcUmEntityModuleType_; prefix = "um"; break;
    }
    std::string name = std::string(isNr ? "nrRlc-" : "rlc-") + prefix + "-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = moduleType->create(name.c_str(), nicModule_);
    // Set the leg's MAC/RLC-mux paths on the COMPOUND before finalize; its NED passes them down
    // to the tx/rx submodules (see '*.macModule = this.macModule' in RlcUmEntityBase/RlcAmEntityBase), so
    // the submodule params carry the right value at build time -- no post-build @mutable write.
    setRlcEntityParams(module, isNr);
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, false, rlcMux, num(id.getDrbId()));

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

    // Wire entity lowerOut gate → RlcMux fromTxEntity
    int fromIdx = rlcMux->gateSize("fromTxEntity");
    rlcMux->setGateSize("fromTxEntity", fromIdx + 1);
    module->gate("lowerOut")->connectTo(rlcMux->gate("fromTxEntity", fromIdx));

    // Wire RlcMux macToTxEntity → entity macIn gate
    int macIdx = rlcMux->gateSize("macToTxEntity");
    rlcMux->setGateSize("macToTxEntity", macIdx + 1);
    rlcMux->gate("macToTxEntity", macIdx)->connectTo(module->gate("macIn"));

    txEnt->setFlowControlInfo(lteInfo);

    // D2D peer tracking for UM TX entities. The LTE-leg (LteRlcUmTxEntity) and
    // NR-leg (NrRlcUmTxEntity) entities both derive from RlcUmTxEntityBase, so the
    // dynamic_cast succeeds for either leg; registration only happens when a D2D
    // mode controller is present (i.e. in D2D-capable configs).
    if (static_cast<LteRlcType>(lteInfo->getRlcType()) == UM) {
        auto *d2dCtrl = inet::findModuleFromPar<D2DModeController>(par("d2dModeControllerModule"), this);
        if (d2dCtrl) {
            if (auto *umTxEnt = dynamic_cast<RlcUmTxEntityBase *>(txEnt))
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

    // Wire RlcMux → entity lowerIn gate
    int idx = rlcMux->gateSize("toRxEntity");
    rlcMux->setGateSize("toRxEntity", idx + 1);
    rlcMux->gate("toRxEntity", idx)->connectTo(module->gate("lowerIn"));

    rxEnt->setFlowControlInfo(lteInfo);

    // Register in mux routing table
    rlcMux->registerRxEntity(id, idx);

    return rxEnt;
}

cModule *BearerManagement::findOrCreatePdcpEntity(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux)
{
    auto it = pdcpEntities_.find(id);
    if (it != pdcpEntities_.end())
        return it->second;

    // Number of legs of this bearer. Two-leg (split-capable) bearers: at a DC UE, every
    // infrastructure bearer (local LTE + local NR stack legs); at a DC master, every UE bearer
    // (local leg + remote leg via X2 to the secondary). Everything else -- non-DC nodes, D2D
    // and multicast bearers, secondaries (X2 relay only) -- is single-leg.
    bool isEnb = (registration_->getNodeType() == NODEB);
    int numLegs = 1;
    if (dualConnectivityEnabled_ && lteInfo->getMulticastGroupId() == NODEID_NONE) {
        if (!isEnb && getNodeTypeById(id.getNodeId()) == NODEB)
            numLegs = 2;
        else if (isEnb && binderModule->getSecondaryNode(registration_->getLteNodeId()) != NODEID_NONE)
            numLegs = 2;
    }

    // Create the per-bearer PDCP entity module (compound: TX + RX sides and, on a multi-leg
    // bearer, the leg splitter/joiner). With duplex bearer establishment the first-processed
    // direction creates it; later calls find it here and just wire their own leg/side.
    std::string name = "pdcp-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = pdcpEntityModuleType_->create(name.c_str(), nicModule_);
    module->par("headerCompressedSize") = par("headerCompressedSize");
    module->par("numLegs") = numLegs;
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, true, rlcMux, num(id.getDrbId()));
    module->scheduleStart(simTime());
    module->callInitialize();

    // DC master: wire the remote (X2) leg to the DcMux right away -- unlike the UE's NR leg,
    // it has no establishment call of its own (the secondary side is an X2 relay only)
    if (isEnb && numLegs == 2) {
        auto *pdcpDcMux = inet::getModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        int dcIdx = pdcpDcMux->gateSize("fromEntity");
        pdcpDcMux->setGateSize("fromEntity", dcIdx + 1);
        module->gate("legOut", 1)->connectTo(pdcpDcMux->gate("fromEntity", dcIdx));
        int rxIdx = pdcpDcMux->gateSize("toRxEntity");
        pdcpDcMux->setGateSize("toRxEntity", rxIdx + 1);
        pdcpDcMux->gate("toRxEntity", rxIdx)->connectTo(module->gate("legIn", 1));
    }

    pdcpEntities_[id] = module;
    return module;
}

// At a DC secondary, the stand-in for a bearer's (master-resident) PDCP entity: one PdcpRelayEntity
// compound per bearer, holding both the DL and the UL relay. The first-processed direction creates
// it; the other finds it here and wires its own half (see createOutgoing/IncomingConnection).
cModule *BearerManagement::findOrCreatePdcpRelayEntity(DrbKey id, RlcMux *rlcMux)
{
    auto it = pdcpRelayEntities_.find(id);
    if (it != pdcpRelayEntities_.end())
        return it->second;

    std::string name = "pdcp-relay-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = pdcpRelayEntityModuleType_->create(name.c_str(), nicModule_);
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, true, rlcMux, num(id.getDrbId()));
    module->scheduleStart(simTime());
    module->callInitialize();

    pdcpRelayEntities_[id] = module;
    return module;
}

// Leg selection for the install functions: at a DC UE, the NR-leg establishment (NR peer ids)
// attaches leg 1 of the bearer's compound, which is keyed by the MASTER node -- the bearer is
// ONE entity whose legs split/rejoin below PDCP. The master-keyed lookup must be precise
// (master node + same DRB id): matching the bare DRB id would also match unrelated bearers of
// this UE, since DRB ids are only unique per peer. Everything else is leg 0 of its own compound.
static int selectPdcpLeg(bool isUe, bool isNr, bool dualConnectivityEnabled, Binder *binder,
                         MacNodeId peerId, DrbKey& compoundId /*inout*/)
{
    if (isUe && isNr && dualConnectivityEnabled && getNodeTypeById(peerId) == NODEB) {
        MacNodeId masterNodeId = binder->getMasterNodeOrSelf(peerId);
        if (masterNodeId != peerId) {  // the peer is a DC secondary node
            compoundId = DrbKey(masterNodeId, compoundId.getDrbId());
            return 1;
        }
    }
    return 0;
}

void BearerManagement::installPdcpTxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
    // The PDCP entity is keyed by dest (id); the RLC entity it wires to is keyed by
    // ctrlInfoToTxDrbKey, which for multicast is the group id -- not the same as id.
    DrbKey rlcId = ctrlInfoToTxDrbKey(lteInfo);

    DrbKey compoundId = id;
    int legIdx = selectPdcpLeg(registration_->getNodeType() == UE, isNr, dualConnectivityEnabled_,
                               binderModule.get(), lteInfo->getDestId(), compoundId);

    cModule *pdcpEnt = findOrCreatePdcpEntity(compoundId, lteInfo, rlcMux);
    if (pdcpEnt->gate("legOut", legIdx)->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpTxSide - TX side of " << compoundId.str() << " leg " << legIdx << " already installed\n";
        return;
    }

    // Wire compound legOut[legIdx] (← tx/splitter) → RLC entity upperIn (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, isNr);
    ASSERT(rlcEnt != nullptr);
    pdcpEnt->gate("legOut", legIdx)->connectTo(rlcEnt->gate("upperIn"));

    if (legIdx == 0) {
        // Wire PdcpMux → compound upperIn (→ tx.in) and register the TX side
        int idx = pdcpMux->gateSize("toTxEntity");
        pdcpMux->setGateSize("toTxEntity", idx + 1);
        pdcpMux->gate("toTxEntity", idx)->connectTo(pdcpEnt->gate("upperIn"));

        auto *txEnt = check_and_cast<PdcpTxEntityBase *>(pdcpEnt->getSubmodule("tx"));
        pdcpMux->registerTxEntity(compoundId, txEnt);
        pdcpTxEntities_[compoundId] = txEnt;
    }
}

void BearerManagement::installPdcpRxSide(DrbKey id, FlowControlInfo *lteInfo, RlcMux *rlcMux, bool isNr)
{
    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
    DrbKey rlcId = ctrlInfoToRxDrbKey(lteInfo);

    DrbKey compoundId = id;
    int legIdx = selectPdcpLeg(registration_->getNodeType() == UE, isNr, dualConnectivityEnabled_,
                               binderModule.get(), lteInfo->getSourceId(), compoundId);

    cModule *pdcpEnt = findOrCreatePdcpEntity(compoundId, lteInfo, rlcMux);
    if (pdcpEnt->gate("legIn", legIdx)->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpRxSide - RX side of " << compoundId.str() << " leg " << legIdx << " already installed\n";
        return;
    }

    // Wire RLC entity upperOut → compound legIn[legIdx] (→ rx/joiner) (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, isNr);
    ASSERT(rlcEnt != nullptr);
    rlcEnt->gate("upperOut")->connectTo(pdcpEnt->gate("legIn", legIdx));

    if (legIdx == 0) {
        // Wire compound upperOut (← rx.out) → PdcpMux fromRxEntity and register the RX side
        int fromIdx = pdcpMux->gateSize("fromRxEntity");
        pdcpMux->setGateSize("fromRxEntity", fromIdx + 1);
        pdcpEnt->gate("upperOut")->connectTo(pdcpMux->gate("fromRxEntity", fromIdx));

        auto *rxEnt = check_and_cast<PdcpRxEntityBase *>(pdcpEnt->getSubmodule("rx"));
        pdcpRxEntities_[compoundId] = rxEnt;
    }
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

    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
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
    // from the PdcpMux routing table where one was installed -- a DC NR leg reuses the master's
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

    // Delete the X2 relay compounds (each deletes its DL and UL relay submodule). DC-secondary
    // only, so the DcMux must exist wherever a relay does.
    ASSERT(pdcpRelayEntities_.empty() || pdcpDcMux != nullptr);
    for (auto it = pdcpRelayEntities_.begin(); it != pdcpRelayEntities_.end(); ) {
        if (!keyed || it->first.getNodeId() == nodeId) {
            it->second->deleteModule();
            it = pdcpRelayEntities_.erase(it);
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
            rlcMux->unregisterRxEntity(it->first);  // no-op if the RX side was never installed
            it->second->deleteModule();
            it = entities.erase(it);
        } else ++it;
    }
}

cModule *BearerManagement::lookupPdcpEntityModule(DrbKey id)
{
    auto it = pdcpEntities_.find(id);
    return it != pdcpEntities_.end() ? it->second : nullptr;
}

cModule *BearerManagement::lookupPdcpRelayEntityModule(DrbKey id)
{
    auto it = pdcpRelayEntities_.find(id);
    return it != pdcpRelayEntities_.end() ? it->second : nullptr;
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
