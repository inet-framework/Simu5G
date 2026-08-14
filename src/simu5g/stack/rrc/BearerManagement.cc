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
#include "simu5g/stack/rrc/Registration.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/ip2nic/Ip2Nic.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/pdcp/PdcpMux.h"
#include "simu5g/stack/pdcp/DcMux.h"
#include "simu5g/stack/pdcp/PdcpTxEntityBase.h"
#include "simu5g/stack/pdcp/PdcpRxEntityBase.h"
#include "simu5g/stack/sdap/NrSdap.h"
#include "simu5g/common/InitStages.h"

namespace simu5g {

using namespace inet;

Define_Module(BearerManagement);

// Build the FlowDescriptor a bearer-establishment call pushes into MAC and stores as the
// RLC entity's FlowControlInfo prototype. FlowControlInfo carries identity only, so this
// is just flow as a FlowDescriptor; bearer configuration (BearerRequest) travels
// separately (LogicalChannelConfig into MAC, the "rlcType" NED param into PDCP).
static FlowDescriptor makeFlowDescriptor(const FlowId& flow)
{
    return FlowDescriptor::fromFlowControlInfo(FlowControlInfo::fromFlowId(flow));
}

// Value for the PDCP entity's "rlcType" NED param (@enum(TM,UM,AM), matching case): unlike
// rlcTypeToA() -- lowercase, used only for logging -- this must be an exact enum literal.
static const char *rlcTypeToParamValue(LteRlcType type)
{
    switch (type) {
        case TM: return "TM";
        case UM: return "UM";
        case AM: return "AM";
        default: throw cRuntimeError("BearerManagement: invalid rlcType %d for PDCP entity", (int)type);
    }
}

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
        drbTableModule.reference(this, "drbTableModule", true);
        rlcMuxModule.reference(this, "rlcMuxModule", true);
        nrRlcMuxModule.reference(this, "nrRlcMuxModule", false);
        macModule.reference(this, "macModule", true);
        nrMacModule.reference(this, "nrMacModule", false);
        sdapModule.reference(this, "sdapModule", false);

        t311_ = par("t311");
        t301_ = par("t301");

        dualConnectivityEnabled_ = par("dualConnectivityEnabled");

        conversationalRlc_ = aToRlcType(par("conversationalRlc"));
        streamingRlc_ = aToRlcType(par("streamingRlc"));
        interactiveRlc_ = aToRlcType(par("interactiveRlc"));
        backgroundRlc_ = aToRlcType(par("backgroundRlc"));
    }
    else if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        // Push the authored DRB configuration into the layers that consume it: SDAP's
        // QFI-to-DRB view and the eNB MAC's per-bearer QoS profiles are working copies
        // those modules never author themselves. (Post-local, so the drbTable has
        // loaded its drbConfig parameter.)
        bool isEnb = (registration_->getNodeType() == NODEB);
        for (const auto& [key, drb] : drbTableModule->getConfiguredDrbs()) {
            if (sdapModule.getNullable() != nullptr)
                sdapModule->configureDrb(drb);
            if (drb.hasQosProfile && isEnb)
                check_and_cast<LteMacEnb *>(macModule.get())->configureDrbQos(key, drb.qos);
        }
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

LteRlcType BearerManagement::qosClassToRlcType(LteTrafficClass qosClass)
{
    switch (qosClass) {
        case CONVERSATIONAL: return conversationalRlc_;
        case INTERACTIVE: return interactiveRlc_;
        case STREAMING: return streamingRlc_;
        case BACKGROUND: return backgroundRlc_;
        default: return backgroundRlc_;  // fallback
    }
}

const DrbDesc *BearerManagement::lookupConfiguredDrb(const FlowId& flow, MacNodeId peerId)
{
    // Authored entries describe infrastructure bearers only; multicast and D2D bearers
    // have DRB ids from other key spaces and must not match them.
    if (flow.multicastGroupId != NODEID_NONE || flow.d2dTxPeerId != NODEID_NONE || flow.d2dRxPeerId != NODEID_NONE)
        return nullptr;
    // Authored entries are keyed by NODEID_NONE ("my serving node") on the UE side,
    // and by the UE's node id on the gNB side.
    MacNodeId cfgPeer = (registration_->getNodeType() == UE) ? NODEID_NONE : peerId;
    return drbTableModule->findConfiguredDrb(DrbKey(cfgPeer, flow.drbId));
}

BearerRequest BearerManagement::resolveBearerRequest(const BearerRequest& reqIn, const FlowId& flow, MacNodeId peerId)
{
    BearerRequest req = reqIn;

    // The authored configuration is RRC's own decision for this bearer and takes
    // precedence; a conflicting explicit request is a configuration error, not a tie
    // to break silently.
    const DrbDesc *cfg = lookupConfiguredDrb(flow, peerId);
    if (cfg && cfg->rlcType != UNKNOWN_RLC_TYPE) {
        if (req.rlcType != UNKNOWN_RLC_TYPE && req.rlcType != cfg->rlcType)
            throw cRuntimeError("Bearer establishment request for DRB %d (peer %d) asks for RLC mode %s, "
                    "but the drbTable's drbConfig authors it as %s -- conflicting configuration",
                    (int)num(flow.drbId), (int)num(peerId),
                    rlcTypeToA(req.rlcType).c_str(), rlcTypeToA(cfg->rlcType).c_str());
        req.rlcType = cfg->rlcType;
    }

    if (req.rlcType == UNKNOWN_RLC_TYPE)
        req.rlcType = qosClassToRlcType(req.qosClass);
    return req;
}

void BearerManagement::createIncomingConnection(const FlowId& flow, const BearerRequest& reqIn, bool withPdcp)
{
    Enter_Method_Silent("createIncomingConnection()");

    // Resolve rlcType == UNKNOWN_RLC_TYPE ("RRC decides") before any use: entity type
    // selection (findOrCreateRlcEntity) and materializeDrb must see only resolved values.
    const BearerRequest req = resolveBearerRequest(reqIn, flow, flow.sourceId);

    EV << "BearerManagement::createIncomingConnection - " << " srcId=" << flow.sourceId << " destId=" << flow.destId
        << " groupId=" << flow.multicastGroupId << " drbId=" << flow.drbId
        << " direction=" << dirToA(flow.direction)
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(flow.destId == registration_->getLteNodeId() || flow.destId == registration_->getNrNodeId() || flow.multicastGroupId != NODEID_NONE);

    // Idempotence guard: with duplex bearer establishment this half may already
    // exist (e.g. re-establishment after a partial teardown); skip instead of
    // crashing on duplicate MAC/RLC/PDCP creation.
    DrbKey rlcId = flow.rxDrbKey();
    StackLeg leg = (registration_->getNodeType()==UE && isNrUe(flow.destId)) ? LEG_NR : LEG_LTE; //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    cModule *existingRlcEnt = lookupRlcEntityModule(rlcId, leg);
    if (existingRlcEnt != nullptr && existingRlcEnt->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::createIncomingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    // RLC entity creation, then the bearer descriptor (needs the RLC entity in place for
    // snFieldLength), then MAC's logical-channel configuration -- all before the MAC
    // connection is created below, since LteMacBase::createOutgoingConnection's lcgMap_
    // insertion reads the pushed configuration back via getLogicalChannelConfig().
    auto *rlcMux = rlcMuxOf(leg);
    installRlcRxSide(rlcId, flow, req, rlcMux, leg);

    const DrbDesc& drb = materializeDrb(flow, req, flow.sourceId, rlcId, leg);

    MacNodeId senderId = flow.sourceId;
    auto mac = (registration_->getNodeType()==UE && isNrUe(flow.destId)) ? nrMacModule.get() : macModule.get(); //TODO FIXME! DOES NOT WORK FOR MULTICAST!!!!!
    LogicalCid lcid = mac->drbIdToLcid(flow.drbId);
    MacCid cid = MacCid(senderId, lcid);
    mac->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});

    // Create MAC incoming connection
    FlowDescriptor desc = makeFlowDescriptor(flow);
    mac->createIncomingConnection(cid, desc);

    // PDCP entity creation (compound: TX+RX, see PdcpEntityBase). At a DC secondary the bearer's
    // PDCP lives at the master, so an PdcpRelayEntity stands in (UL half wired here).
    if (withPdcp) {
        DrbKey id = DrbKey(flow.sourceId, flow.drbId);
        installPdcpRxSide(id, flow, drb.rlcType, rlcMux, leg);
    }
    else {
        // DC secondary node: forward the UL PDU from RLC to the master over X2 unprocessed
        // (PdcpRelayEntity's UL half, see PdcpRelayEntity).
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // relay entities are eNB-only
        DrbKey id = DrbKey(flow.sourceId, flow.drbId);
        cModule *relay = findOrCreatePdcpRelayEntity(id, rlcMux);

        // Wire RLC entity upperOut → relay legIn[0] (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, leg);
        ASSERT(rlcEnt2 != nullptr);
        rlcEnt2->gate("upperOut")->connectTo(relay->gate("legIn", 0));

        // Wire relay x2Out → DcMux (UL PDU tunneled to the master)
        int fromIdx = pdcpDcMux->gateSize("fromEntity");
        pdcpDcMux->setGateSize("fromEntity", fromIdx + 1);
        relay->gate("x2Out")->connectTo(pdcpDcMux->gate("fromEntity", fromIdx));
    }
}

void BearerManagement::createOutgoingConnection(const FlowId& flow, const BearerRequest& reqIn, bool withPdcp)
{
    Enter_Method_Silent("createOutgoingConnection()");

    // Resolve rlcType == UNKNOWN_RLC_TYPE ("RRC decides") before any use: entity type
    // selection (findOrCreateRlcEntity) and materializeDrb must see only resolved values.
    const BearerRequest req = resolveBearerRequest(reqIn, flow, flow.destId);

    EV << "BearerManagement::createOutgoingConnection - " << " srcId=" << flow.sourceId << " destId=" << flow.destId
        << " groupId=" << flow.multicastGroupId << " drbId=" << flow.drbId
        << " direction=" << dirToA(flow.direction)
        << " withPdcp=" << (withPdcp ? "yes" : "no") << endl;

    ASSERT(flow.sourceId == registration_->getLteNodeId() || flow.sourceId == registration_->getNrNodeId());

    // Idempotence guard: with duplex bearer establishment this half may already
    // exist (e.g. re-establishment after a partial teardown); skip instead of
    // crashing on duplicate MAC/RLC/PDCP creation.
    DrbKey rlcId = flow.txDrbKey();
    StackLeg leg = (registration_->getNodeType()==UE && isNrUe(flow.sourceId)) ? LEG_NR : LEG_LTE;
    cModule *existingRlcEnt = lookupRlcEntityModule(rlcId, leg);
    if (existingRlcEnt != nullptr && existingRlcEnt->gate("lowerOut")->isConnectedOutside()) {
        EV << "BearerManagement::createOutgoingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    // RLC entity creation, then the bearer descriptor (needs the RLC entity in place for
    // snFieldLength), then MAC's logical-channel configuration -- all before the MAC
    // connection is created below, since LteMacBase::createOutgoingConnection's lcgMap_
    // insertion reads the pushed configuration back via getLogicalChannelConfig().
    auto *rlcMux = rlcMuxOf(leg);
    installRlcTxSide(rlcId, flow, req, rlcMux, leg);

    const DrbDesc& drb = materializeDrb(flow, req, flow.destId, rlcId, leg);

    MacNodeId destId = flow.destId;
    auto mac = (registration_->getNodeType()==UE && isNrUe(flow.sourceId)) ? nrMacModule.get() : macModule.get();
    LogicalCid lcid = mac->drbIdToLcid(flow.drbId);
    MacCid cid = MacCid(destId, lcid);
    mac->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});

    // Create MAC outgoing connection
    FlowDescriptor desc = makeFlowDescriptor(flow);
    mac->createOutgoingConnection(cid, desc);

    // PDCP entity creation (compound: TX+RX, see PdcpEntityBase). At a DC secondary the bearer's
    // PDCP lives at the master, so an PdcpRelayEntity stands in (DL half wired here).
    if (withPdcp) {
        DrbKey id = DrbKey(flow.destId, flow.drbId);
        installPdcpTxSide(id, flow, drb.rlcType, rlcMux, leg);
    }
    else {
        // DC secondary node: the DL PDU arrives already PDCP-processed from the master over X2;
        // forward it straight to RLC (PdcpRelayEntity's DL half, see PdcpRelayEntity).
        auto *pdcpDcMux = inet::findModuleFromPar<DcMux>(par("pdcpDcMuxModule"), this);
        ASSERT(pdcpDcMux != nullptr); // relay entities are eNB-only
        DrbKey id = DrbKey(flow.destId, flow.drbId);
        cModule *relay = findOrCreatePdcpRelayEntity(id, rlcMux);

        // Wire DcMux → relay x2In (DcMux dispatches the incoming DL X2 PDU)
        int idx = pdcpDcMux->gateSize("toRelayEntity");
        pdcpDcMux->setGateSize("toRelayEntity", idx + 1);
        pdcpDcMux->gate("toRelayEntity", idx)->connectTo(relay->gate("x2In"));

        // Wire relay legOut[0] → RLC entity upperIn (direct per-DRB connection)
        cModule *rlcEnt2 = lookupRlcEntityModule(rlcId, leg);
        ASSERT(rlcEnt2 != nullptr);
        relay->gate("legOut", 0)->connectTo(rlcEnt2->gate("upperIn"));
    }
}

std::map<DrbKey, cModule *>& BearerManagement::rlcEntitiesOf(StackLeg leg)
{
    switch (leg) {
        case LEG_NR: return nrRlcEntities_;
        case LEG_SL: return slRlcEntities_;
        default: return rlcEntities_;
    }
}

std::map<DrbKey, cModule *>& BearerManagement::pdcpEntitiesOf(StackLeg leg)
{
    return leg == LEG_SL ? slPdcpEntities_ : pdcpEntities_;
}

RlcMux *BearerManagement::rlcMuxOf(StackLeg leg)
{
    switch (leg) {
        case LEG_NR: return nrRlcMuxModule.get();
        case LEG_SL: resolveSlModules(); return slRlcMux_;
        default: return rlcMuxModule.get();
    }
}

LteMacBase *BearerManagement::macOf(StackLeg leg)
{
    switch (leg) {
        case LEG_NR: return nrMacModule.get();
        case LEG_SL: resolveSlModules(); return slMac_;
        default: return macModule.get();
    }
}

const char *BearerManagement::legPrefix(StackLeg leg)
{
    switch (leg) {
        case LEG_NR: return "nrRlc-";
        case LEG_SL: return "slRlc-";
        default: return "rlc-";
    }
}

void BearerManagement::setRlcEntityParams(cModule *entity, StackLeg leg)
{
    // 'entity' is the RlcTm/Um/AmEntity COMPOUND; these paths are relative to it (^ = the NIC).
    // The compound NED absPath()-resolves them and passes them to its tx/rx submodules (see
    // '*.macModule = default(absPath(this.macModule))'). hasPar() guards because not every
    // compound/leg declares both (TM has neither; AM has no rlcMux). Per leg: a UE's NR leg uses
    // nrMac/nrRlcMux; everything else (incl. a gNB's NR bearers, which have no nrMac) uses mac/rlcMux.
    if (entity->hasPar("macModule"))
        entity->par("macModule").setStringValue(leg == LEG_NR ? "^.nrMac" : leg == LEG_SL ? "^.slMac" : "^.mac");
    if (entity->hasPar("rlcMuxModule"))
        entity->par("rlcMuxModule").setStringValue(leg == LEG_NR ? "^.nrRlcMux" : leg == LEG_SL ? "^.slRlcMux" : "^.rlcMux");
    // The RLC entity's wire format is not a parameter: it is inherent to the entity
    // class selected per bearer in findOrCreateRlcEntity() (NR-SO vs LTE-FI, by the same
    // RAT predicate as here), and RRC records the choice in the bearer's descriptor
    // (DrbDesc::soFraming, see materializeDrb()). (The NR-leg marker proper is the
    // ip2nic.isNr parameter, not set here.)
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

cModule *BearerManagement::lookupRlcEntityModule(DrbKey id, StackLeg leg)
{
    auto& entities = rlcEntitiesOf(leg);
    auto it = entities.find(id);
    return it != entities.end() ? it->second : nullptr;
}

cModule *BearerManagement::findOrCreateRlcEntity(DrbKey id, LteRlcType rlcType, const FlowId& flow, RlcMux *rlcMux, StackLeg leg)
{
    auto& entities = rlcEntitiesOf(leg);
    auto it = entities.find(id);
    if (it != entities.end())
        return it->second;

    // Create the per-bearer RLC entity module (compound: TX + RX sides). With duplex
    // bearer establishment the first-processed direction creates it; the other
    // direction finds it here and just installs (wires) its own side.
    // The entity TYPE (LTE-FI vs NR-SO wire format) must be identical at both ends of
    // the bearer, so it keys on whether the bearer's UE is an NR UE — a symmetric
    // property of the source/dest node ids — not on the local-node isNr flag (which is
    // only ever true at a UE, leaving the gNB on the LTE type and breaking the wire).
    // For a DC UE this still separates the NR-secondary leg from the LTE-master leg,
    // since those bearers reference the UE's NR vs LTE node id respectively.
    bool isNrBearer = isNrUe(flow.sourceId) || isNrUe(flow.destId);
    cModuleType *moduleType;
    const char *prefix;
    switch (rlcType) {
        case TM:
            moduleType = rlcTmEntityModuleType_;
            prefix = "tm";
            break;
        case AM:
            moduleType = leg == LEG_SL ? slRlcAmEntityModuleType_
                                       : (isNrBearer ? nrRlcAmEntityModuleType_ : lteRlcAmEntityModuleType_);
            prefix = "am";
            break;
        default:
            moduleType = leg == LEG_SL ? slRlcUmEntityModuleType_
                                       : (isNrBearer ? nrRlcUmEntityModuleType_ : lteRlcUmEntityModuleType_);
            prefix = "um";
            break;
    }
    std::string name = std::string(legPrefix(leg)) + prefix + "-" + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = moduleType->create(name.c_str(), nicModule_);
    // Set the leg's MAC/RLC-mux paths on the COMPOUND before finalize; its NED passes them down
    // to the tx/rx submodules (see '*.macModule = this.macModule' in RlcUmEntityBase/RlcAmEntityBase), so
    // the submodule params carry the right value at build time -- no post-build @mutable write.
    setRlcEntityParams(module, leg);
    module->finalizeParameters();
    module->buildInside();
    setEntityDisplayPosition(module, false, rlcMux, num(id.getDrbId()));

    module->scheduleStart(simTime());
    module->callInitialize();

    entities[id] = module;
    return module;
}

RlcTxEntityBase *BearerManagement::installRlcTxSide(DrbKey id, const FlowId& flow, const BearerRequest& req, RlcMux *rlcMux, StackLeg leg)
{
    cModule *module = findOrCreateRlcEntity(id, req.rlcType, flow, rlcMux, leg);
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

    FlowDescriptor proto = makeFlowDescriptor(flow);
    txEnt->setFlowControlInfo(&proto); // note: a D2D UM TX entity also registers itself with the D2D mode controller here


    return txEnt;
}

RlcRxEntityBase *BearerManagement::installRlcRxSide(DrbKey id, const FlowId& flow, const BearerRequest& req, RlcMux *rlcMux, StackLeg leg)
{
    cModule *module = findOrCreateRlcEntity(id, req.rlcType, flow, rlcMux, leg);
    auto *rxEnt = check_and_cast<RlcRxEntityBase *>(module->getSubmodule("rx"));

    if (module->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::installRlcRxSide - RX side of " << id.str() << " already installed\n";
        return rxEnt;
    }

    // Wire RlcMux → entity lowerIn gate
    int idx = rlcMux->gateSize("toRxEntity");
    rlcMux->setGateSize("toRxEntity", idx + 1);
    rlcMux->gate("toRxEntity", idx)->connectTo(module->gate("lowerIn"));

    FlowDescriptor proto = makeFlowDescriptor(flow);
    rxEnt->setFlowControlInfo(&proto);

    // Register in mux routing table
    rlcMux->registerRxEntity(id, idx);

    return rxEnt;
}

// Records the configuration of the bearer being established, keyed by the peer node and
// the DRB id, so that the two directions of the duplex establishment fill in the same
// descriptor. At a DC UE the NR leg is established against the secondary node and so gets
// a descriptor of its own; joining the legs of a split bearer into one descriptor is what
// the legs table will do.
const DrbDesc& BearerManagement::materializeDrb(const FlowId& flow, const BearerRequest& req, MacNodeId peerId, DrbKey rlcId, StackLeg leg)
{
    DrbDesc& drb = drbTableModule->getOrCreateDrb(DrbKey(peerId, flow.drbId));
    drb.lcid = LogicalCid(num(flow.drbId));   // the 1:1 mapping of LteMacBase::drbIdToLcid
    drb.lcg = req.qosClass;
    drb.rlcType = req.rlcType;

    // soFraming is RRC's own decision: the RAT+mode predicate that also selects the
    // entity type (NR UM gets the SO/no-concat framing of TS 38.322; everything else --
    // LTE, and NR AM, whose MAC-side SO multiplexing MAC does not implement -- gets the
    // FI/concatenation framing of TS 36.322).
    bool isNrBearer = isNrUe(flow.sourceId) || isNrUe(flow.destId);
    drb.soFraming = (drb.rlcType == UM) && isNrBearer;

    // The SN field length, in contrast, is transcribed off the RLC entity serving the
    // bearer: its source of truth is the entity's own parameters (NrRlcUmTxEntity's
    // sn_FieldLength, NR-AM's AM_Window_Size derivation), not a bearer-level decision.
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, leg);
    ASSERT(rlcEnt != nullptr);   // the RLC entity is installed before this is called
    auto *txEnt = check_and_cast<RlcTxEntityBase *>(rlcEnt->getSubmodule("tx"));
    drb.snFieldLength = txEnt->snFieldLength();

    // The PC5 half is only ever set by a sidelink establishment call.
    drb.slCastType = req.slCastType;
    drb.slPqi = req.slPqi;

    // The SDAP half and the QoS profile come from the authored configuration, if any,
    // so the established descriptor is the complete record of the bearer.
    if (const DrbDesc *cfg = lookupConfiguredDrb(flow, peerId)) {
        drb.pduSessionType = cfg->pduSessionType;
        drb.upperProtocol = cfg->upperProtocol;
        drb.qfiList = cfg->qfiList;
        drb.isDefault = cfg->isDefault;
        drb.hasQosProfile = cfg->hasQosProfile;
        drb.qos = cfg->qos;
    }

    EV << "BearerManagement::materializeDrb - " << drb << endl;
    return drb;
}

int BearerManagement::getNumLegs(DrbKey id, const FlowId& flow, StackLeg leg)
{
    // A sidelink bearer is always single-leg: PC5 has no dual connectivity.
    if (leg == LEG_SL)
        return 1;
    // Number of legs of this bearer. Two-leg (split-capable) bearers: at a DC UE, every
    // infrastructure bearer (local LTE + local NR stack legs); at a DC master, every UE bearer
    // (local leg + remote leg via X2 to the secondary). Everything else -- non-DC nodes, D2D
    // and multicast bearers, secondaries (X2 relay only) -- is single-leg.
    bool isEnb = (registration_->getNodeType() == NODEB);
    if (dualConnectivityEnabled_ && flow.multicastGroupId == NODEID_NONE) {
        if (!isEnb && getNodeTypeById(id.getNodeId()) == NODEB)
            return 2;
        else if (isEnb && binderModule->getSecondaryNode(registration_->getLteNodeId()) != NODEID_NONE)
            return 2;
    }
    return 1;
}

cModule *BearerManagement::findOrCreatePdcpEntity(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg)
{
    auto& entities = pdcpEntitiesOf(leg);
    auto it = entities.find(id);
    if (it != entities.end())
        return it->second;

    bool isEnb = (registration_->getNodeType() == NODEB);
    int numLegs = getNumLegs(id, flow, leg);

    // Create the per-bearer PDCP entity module (compound: TX + RX sides and, on a multi-leg
    // bearer, the leg splitter/joiner). With duplex bearer establishment the first-processed
    // direction creates it; later calls find it here and just wire their own leg/side.
    std::string name = std::string(leg == LEG_SL ? "slPdcp-" : "pdcp-") + std::to_string(num(id.getNodeId())) + "-" + std::to_string(num(id.getDrbId()));
    auto *module = (leg == LEG_SL ? slPdcpEntityModuleType_ : pdcpEntityModuleType_)->create(name.c_str(), nicModule_);
    module->par("headerCompressedSize") = par("headerCompressedSize");
    module->par("numLegs") = numLegs;
    module->par("rlcType") = rlcTypeToParamValue(rlcType);
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

    entities[id] = module;
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
int BearerManagement::selectPdcpLeg(StackLeg leg, MacNodeId peerId, DrbKey& compoundId /*inout*/)
{
    bool isUe = (registration_->getNodeType() == UE);
    if (isUe && leg == LEG_NR && dualConnectivityEnabled_ && getNodeTypeById(peerId) == NODEB) {
        MacNodeId masterNodeId = binderModule->getMasterNodeOrSelf(peerId);
        if (masterNodeId != peerId) {  // the peer is a DC secondary node
            compoundId = DrbKey(masterNodeId, compoundId.getDrbId());
            return 1;
        }
    }
    return 0;
}

void BearerManagement::installPdcpTxSide(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg)
{
    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
    // The PDCP entity is keyed by dest (id); the RLC entity it wires to is keyed by
    // flow.txDrbKey(), which for multicast is the group id -- not the same as id.
    DrbKey rlcId = flow.txDrbKey();

    DrbKey compoundId = id;
    int legIdx = selectPdcpLeg(leg, flow.destId, compoundId);

    cModule *pdcpEnt = findOrCreatePdcpEntity(compoundId, flow, rlcType, rlcMux, leg);
    if (pdcpEnt->gate("legOut", legIdx)->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpTxSide - TX side of " << compoundId.str() << " leg " << legIdx << " already installed\n";
        return;
    }

    // Wire compound legOut[legIdx] (← tx/splitter) → RLC entity upperIn (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, leg);
    ASSERT(rlcEnt != nullptr);
    pdcpEnt->gate("legOut", legIdx)->connectTo(rlcEnt->gate("upperIn"));

    if (legIdx == 0) {
        // Wire PdcpMux → compound upperIn (→ tx.in) and register the TX side
        int idx = pdcpMux->gateSize("toTxEntity");
        pdcpMux->setGateSize("toTxEntity", idx + 1);
        pdcpMux->gate("toTxEntity", idx)->connectTo(pdcpEnt->gate("upperIn"));
        pdcpMux->registerTxEntity(compoundId, idx);

        auto *txEnt = check_and_cast<PdcpTxEntityBase *>(pdcpEnt->getSubmodule("tx"));
        pdcpTxEntities_[compoundId] = txEnt;
    }
}

void BearerManagement::installPdcpRxSide(DrbKey id, const FlowId& flow, LteRlcType rlcType, RlcMux *rlcMux, StackLeg leg)
{
    auto *pdcpMux = inet::getModuleFromPar<PdcpMux>(par("pdcpMuxModule"), this);
    DrbKey rlcId = flow.rxDrbKey();

    DrbKey compoundId = id;
    int legIdx = selectPdcpLeg(leg, flow.sourceId, compoundId);

    cModule *pdcpEnt = findOrCreatePdcpEntity(compoundId, flow, rlcType, rlcMux, leg);
    if (pdcpEnt->gate("legIn", legIdx)->isConnectedOutside()) {
        EV << "BearerManagement::installPdcpRxSide - RX side of " << compoundId.str() << " leg " << legIdx << " already installed\n";
        return;
    }

    // Wire RLC entity upperOut → compound legIn[legIdx] (→ rx/joiner) (direct per-DRB connection)
    cModule *rlcEnt = lookupRlcEntityModule(rlcId, leg);
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

RlcTxEntityBase *BearerManagement::createRlcTxBuffer(DrbKey id, const FlowId& flow, const BearerRequest& reqIn)
{
    Enter_Method_Silent("createRlcTxBuffer()");
    const BearerRequest req = resolveBearerRequest(reqIn, flow, flow.destId);
    RlcTxEntityBase *txEnt = installRlcTxSide(id, flow, req, rlcMuxModule.get(), LEG_LTE);
    const DrbDesc& drb = materializeDrb(flow, req, flow.destId, id, LEG_LTE);
    LteMacBase *mac = macModule.get();
    MacCid cid = MacCid(flow.destId, mac->drbIdToLcid(flow.drbId));
    mac->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});
    return txEnt;
}

RlcRxEntityBase *BearerManagement::createRlcRxBuffer(DrbKey id, const FlowId& flow, const BearerRequest& reqIn)
{
    Enter_Method_Silent("createRlcRxBuffer()");
    const BearerRequest req = resolveBearerRequest(reqIn, flow, flow.sourceId);
    RlcRxEntityBase *rxEnt = installRlcRxSide(id, flow, req, rlcMuxModule.get(), LEG_LTE);
    const DrbDesc& drb = materializeDrb(flow, req, flow.sourceId, id, LEG_LTE);
    LteMacBase *mac = macModule.get();
    MacCid cid = MacCid(flow.sourceId, mac->drbIdToLcid(flow.drbId));
    mac->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});
    return rxEnt;
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

    // The bearer's configuration lives exactly as long as its PDCP entity: a bearer is
    // established on demand, so a stale descriptor would be picked up by the next
    // establishment instead of being rebuilt from the new link's parameters.
    if (keyed)
        drbTableModule->removeDrbsOfPeer(nodeId);
    else
        drbTableModule->removeAllDrbs();

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
    // Search every leg. Only an INSTALLED (mux-wired) TX side counts: with the
    // compound entity, the RX-side install may have created the module while the
    // TX side is not set up yet -- callers create it via createRlcTxBuffer then.
    for (auto *entities : { &rlcEntities_, &nrRlcEntities_, &slRlcEntities_ }) {
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

// ---------------------------------------------------------------------------
// Sidelink bearers
//
// Built on the PC5 leg (slMac / slRlcMux) through the same entity install path
// as the Uu legs, so an SLRB is an ordinary bearer whose descriptor happens to
// carry a PC5 half. The sidelink RLC entities are the NR (SI/SO) profiles --
// sidelink RLC is TS 38.322 -- while PDCP stays on the LTE profile, whose TX
// side does not rewrite a UE's source id to its NR node id, which would destroy
// the PC5 addressing the sidelink keying is built on.
// ---------------------------------------------------------------------------

void BearerManagement::resolveSlModules()
{
    if (slMac_ != nullptr)
        return;
    slRlcMux_ = check_and_cast<RlcMux *>(getModuleByPath(par("slRlcMuxModule").stringValue()));
    slMac_ = check_and_cast<LteMacBase *>(getModuleByPath(par("slMacModule").stringValue()));
    slPdcpEntityModuleType_ = cModuleType::get(par("slPdcpEntityModuleType").stringValue());
    slRlcUmEntityModuleType_ = cModuleType::get(par("slRlcUmEntityModuleType").stringValue());
    slRlcAmEntityModuleType_ = cModuleType::get(par("slRlcAmEntityModuleType").stringValue());
    slSdapEntityModuleType_ = cModuleType::get(par("slSdapEntityModuleType").stringValue());
}

void BearerManagement::createSlOutgoingConnection(const FlowId& flow, const BearerRequest& req)
{
    Enter_Method_Silent("createSlOutgoingConnection()");
    ASSERT(flow.direction == SL);
    resolveSlModules();

    EV << "BearerManagement::createSlOutgoingConnection - srcId=" << flow.sourceId
       << " destId=" << flow.destId << " drbId=" << flow.drbId
       << " cast=" << slCastTypeToA(req.slCastType) << endl;

    // Idempotence guard, as on the Uu legs: a symmetric sidelink bearer or a
    // re-established link may have created this half already.
    DrbKey rlcId = flow.txDrbKey();
    cModule *existing = lookupRlcEntityModule(rlcId, LEG_SL);
    if (existing != nullptr && existing->gate("lowerOut")->isConnectedOutside()) {
        EV << "BearerManagement::createSlOutgoingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    installRlcTxSide(rlcId, flow, req, slRlcMux_, LEG_SL);
    const DrbDesc& drb = materializeDrb(flow, req, flow.destId, rlcId, LEG_SL);

    MacCid cid = MacCid(flow.destId, slMac_->drbIdToLcid(flow.drbId));
    slMac_->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});
    slMac_->createOutgoingConnection(cid, makeFlowDescriptor(flow));

    installPdcpTxSide(DrbKey(flow.destId, flow.drbId), flow, drb.rlcType, slRlcMux_, LEG_SL);
}

void BearerManagement::createSlIncomingConnection(const FlowId& flow, const BearerRequest& req)
{
    Enter_Method_Silent("createSlIncomingConnection()");
    ASSERT(flow.direction == SL);
    resolveSlModules();

    EV << "BearerManagement::createSlIncomingConnection - srcId=" << flow.sourceId
       << " destId=" << flow.destId << " drbId=" << flow.drbId
       << " cast=" << slCastTypeToA(req.slCastType) << endl;

    DrbKey rlcId = flow.rxDrbKey();
    cModule *existing = lookupRlcEntityModule(rlcId, LEG_SL);
    if (existing != nullptr && existing->gate("lowerIn")->isConnectedOutside()) {
        EV << "BearerManagement::createSlIncomingConnection - entities for " << rlcId.str() << " already exist, skipping\n";
        return;
    }

    installRlcRxSide(rlcId, flow, req, slRlcMux_, LEG_SL);
    const DrbDesc& drb = materializeDrb(flow, req, flow.sourceId, rlcId, LEG_SL);

    MacCid cid = MacCid(flow.sourceId, slMac_->drbIdToLcid(flow.drbId));
    slMac_->configureLogicalChannel(cid, LogicalChannelConfig{drb.rlcType, drb.soFraming, drb.snFieldLength, drb.lcg, drb.slCastType, drb.slPqi});
    slMac_->createIncomingConnection(cid, makeFlowDescriptor(flow));

    installPdcpRxSide(DrbKey(flow.sourceId, flow.drbId), flow, drb.rlcType, slRlcMux_, LEG_SL);
}

int BearerManagement::createSlSrbConnection(const FlowId& out, const FlowId& in, cModule *slRrcModule)
{
    Enter_Method_Silent("createSlSrbConnection()");
    ASSERT(out.direction == SL && in.direction == SL);
    resolveSlModules();

    MacNodeId peerId = out.destId;
    EV << "BearerManagement::createSlSrbConnection - transparent-mode SL-SRB (drb "
       << num(out.drbId) << ") toward peer " << peerId << endl;

    BearerRequest req;
    req.rlcType = TM;
    req.slCastType = SL_UNICAST;

    // MAC connections both ways: the signalling bearer is bidirectional by construction
    MacCid outCid = MacCid(peerId, slMac_->drbIdToLcid(out.drbId));
    MacCid inCid = MacCid(peerId, slMac_->drbIdToLcid(in.drbId));
    slMac_->configureLogicalChannel(outCid, LogicalChannelConfig{TM, false, 0, req.qosClass, SL_UNICAST, 0});
    slMac_->configureLogicalChannel(inCid, LogicalChannelConfig{TM, false, 0, req.qosClass, SL_UNICAST, 0});
    slMac_->createOutgoingConnection(outCid, makeFlowDescriptor(out));
    slMac_->createIncomingConnection(inCid, makeFlowDescriptor(in));

    // Both directions of a symmetric bearer key to (peer, drb), so one entity
    // compound carries them.
    DrbKey id = out.txDrbKey();
    ASSERT(id == in.rxDrbKey());
    installRlcTxSide(id, out, req, slRlcMux_, LEG_SL);
    installRlcRxSide(id, in, req, slRlcMux_, LEG_SL);
    cModule *entity = lookupRlcEntityModule(id, LEG_SL);

    // PC5-RRC skips PDCP: the SlRrc module takes that place on this bearer, one
    // srbOut/srbIn gate pair per link.
    int rrcIdx = slRrcModule->gateSize("srbOut");
    slRrcModule->setGateSize("srbOut", rrcIdx + 1);
    slRrcModule->setGateSize("srbIn", rrcIdx + 1);
    slRrcModule->gate("srbOut", rrcIdx)->connectTo(entity->gate("upperIn"));
    entity->gate("upperOut")->connectTo(slRrcModule->gate("srbIn", rrcIdx));

    return rrcIdx;
}

int BearerManagement::createSlSdapEntity(bool tx, uint32_t peerKey, cModule *slIp2Nic)
{
    Enter_Method_Silent("createSlSdapEntity()");
    resolveSlModules();

    const char *outVec = tx ? "sdapTxOut" : "sdapRxOut";
    const char *inVec = tx ? "sdapTxIn" : "sdapRxIn";

    std::string name = std::string("slSdap-") + (tx ? "tx-" : "rx-") + std::to_string(peerKey);
    cModule *module = slSdapEntityModuleType_->create(name.c_str(), nicModule_);
    module->par("role").setStringValue(tx ? "tx" : "rx");
    module->par("peerKey").setIntValue((intval_t)peerKey);
    module->finalizeParameters();
    module->buildInside();

    int idx = slIp2Nic->gateSize(outVec);
    slIp2Nic->setGateSize(outVec, idx + 1);
    slIp2Nic->setGateSize(inVec, idx + 1);
    slIp2Nic->gate(outVec, idx)->connectTo(module->gate("in"));
    module->gate("out")->connectTo(slIp2Nic->gate(inVec, idx));

    module->scheduleStart(simTime());
    module->callInitialize();

    EV << "BearerManagement::createSlSdapEntity - created " << name << " (gate index " << idx << ")" << endl;
    return idx;
}

} // namespace simu5g
