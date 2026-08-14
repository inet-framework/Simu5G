//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/rrc/SlRrc.h"

#include <set>

#include <inet/common/InitStages.h>
#include <inet/common/ModuleAccess.h>
#include <omnetpp/cvaluemap.h>

#include "simu5g/common/InitStages.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/sidelink/ip2nic/SlIp2Nic.h"
#include "simu5g/stack/sidelink/mac/NrSlMacUe.h"
#include "simu5g/stack/sidelink/rrc/SlGnbRrc.h"
#include "simu5g/stack/sidelink/rrc/SlPc5Rrc_m.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(SlRrc);

int SlRrc::numInitStages() const
{
    return inet::NUM_INIT_STAGES;
}

void SlRrc::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        cModule *node = inet::getContainingNode(this);
        nodeId_ = MacNodeId(node->par("nrMacNodeId").intValue());

        // parse the preconfiguration (pool + slrbConfig)
        auto *cfg = check_and_cast<cValueMap *>(par("preconfig").objectValue());
        preconfig_.loadFromJson(cfg);

        // D3 invariant: DRB ids must be unique per destination L2 ID (2-field
        // DrbKey stays collision-free because TX keys use the destination L2Pid)
        std::set<int> drbs;
        for (const auto& e : preconfig_.slrbConfig)
            if (!drbs.insert(num(e.drbId)).second)
                throw cRuntimeError("SlRrc: duplicate DRB id %hu in slrbConfig (DRB ids must be unique, see D3)", num(e.drbId));

        // own source L2 ID: explicit parameter, or derived from the node id
        long l2IdPar = par("srcL2Id").intValue();
        srcL2Id_ = (l2IdPar >= 0) ? (SlL2Id)l2IdPar : (SlL2Id)num(nodeId_);

        bearerManagement_ = check_and_cast<BearerManagement *>(getModuleByPath(par("bearerManagementModule").stringValue()));
        overTheAir_ = (par("pc5RrcMode").stdstringValue() == "overTheAir");

        // registrations with the global SL registry
        slBinder_ = SlBinder::getInstance();
        slBinder_->registerUeL2Id(srcL2Id_, nodeId_);
        slBinder_->registerSlRrc(nodeId_, this);

        // D32 (SL-3): the shared Uu/SL radio-state object consulted by both
        // PHY legs when the half-duplex arbiter (sharedUuSlRadio) is on
        slBinder_->registerUeRadioState(nodeId_, new SlUeRadioState());
        for (const auto& e : preconfig_.slrbConfig) {
            if (e.castType == SL_BROADCAST || e.castType == SL_GROUPCAST) {
                slBinder_->getOrAssignGroupL2Pid(e.dstL2Id);
                // SL-1: every SL UE with this SLRB configured listens to the destination
                slBinder_->joinGroup(e.dstL2Id, nodeId_);
            }
            if (!e.destAddress.empty())
                slBinder_->registerMulticastAddress(inet::Ipv4Address(e.destAddress.c_str()), e.dstL2Id);
        }

        // D25 (SL-3): pool resolution (and the SL carrier registration) moved
        // to resolvePool() at INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION - the
        // serving cell is only final after the HandoverController's
        // INITSTAGE_SIMU5G_PHYSICAL_LAYER attach (G18)
        poolFromServingCell_ = (par("poolSource").stdstringValue() == "servingCell");
        binder_.reference(this, "binderModule", true);

        EV << "SlRrc::initialize - node " << nodeId_ << ", srcL2Id " << srcL2Id_
           << ", " << preconfig_.slrbConfig.size() << " SLRB(s) configured, poolSource "
           << par("poolSource").stringValue() << endl;
    }
    else if (stage == INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION) {
        resolvePool();
    }
}

void SlRrc::resolvePool()
{
    // D25 ("genie SIB12"): with poolSource="servingCell", an attached UE
    // copies the serving cell's pool section over the local preconfig's; a
    // detached UE falls back to the local preconfig (models the spec's
    // out-of-coverage preconfiguration, so mixed-coverage scenarios work
    // with one config). Only the pool section is cell-provided - SLRB/QoS/
    // unicast sections stay UE-local (SIB12 carries pools, not app bearers).
    // Consumers (NrSlMacUe, NrSlPhyUe) cache pool geometry at
    // INITSTAGE_SIMU5G_TTI_SETUP, strictly after this stage.
    if (poolFromServingCell_) {
        MacNodeId servingCell = binder_->getServingNode(nodeId_);
        if (servingCell != NODEID_NONE) {
            SlGnbRrc *slGnbRrc = slBinder_->getSlGnbRrc(servingCell);
            if (slGnbRrc == nullptr)
                throw cRuntimeError("SlRrc: poolSource=\"servingCell\" but serving cell %hu has no "
                                    "sidelink support (set hasSidelink=true on the gNodeB and configure "
                                    "its slGnbRrc.slPoolConfig)", num(servingCell));

            const SlPreconfig& cell = slGnbRrc->getPoolConfig();
            preconfig_.carrierFrequencyGHz = cell.carrierFrequencyGHz;
            preconfig_.numerologyIndex = cell.numerologyIndex;
            preconfig_.subchannelSize = cell.subchannelSize;
            preconfig_.numSubchannels = cell.numSubchannels;
            preconfig_.slotBitmap = cell.slotBitmap;
            preconfig_.t0Ms = cell.t0Ms;
            preconfig_.t1 = cell.t1;
            preconfig_.t2 = cell.t2;
            preconfig_.rsrpThresholdDbm = cell.rsrpThresholdDbm;
            preconfig_.reservationPeriodsMs = cell.reservationPeriodsMs;
            preconfig_.blindRetx = cell.blindRetx;
            preconfig_.psfchPeriod = cell.psfchPeriod;
            preconfig_.psfchMinGap = cell.psfchMinGap;
            preconfig_.psfchResources = cell.psfchResources;

            slGnbRrc->registerSlUe(nodeId_);

            EV << "SlRrc::resolvePool - node " << nodeId_ << ": pool provisioned from serving cell "
               << servingCell << " (carrier " << preconfig_.carrierFrequencyGHz << " GHz, mu="
               << preconfig_.numerologyIndex << ", " << preconfig_.numSubchannels << " subchannels)" << endl;

            // configured-grant request (D30, WP-P): reserve the standing
            // train at the cell and hand it to the SL MAC (type 1 activates
            // immediately at the MAC's pool-init stage; type 2 stays dormant
            // until a cgAction=activate DCI)
            auto *cgCfg = check_and_cast<cValueMap *>(par("configuredGrant").objectValue());
            if (cgCfg->containsKey("type")) {
                int type = (int)cgCfg->get("type").intValue();
                int periodMs = (int)cgCfg->get("periodMs").intValue();
                int tbBytes = (int)cgCfg->get("tbBytes").intValue();

                SlEnbScheduler::GrantSpec spec = slGnbRrc->reserveConfiguredGrant(nodeId_, type, periodMs, tbBytes);

                auto *slMac = dynamic_cast<NrSlMacUe *>(getModuleByPath(par("slMacModule").stringValue()));
                if (slMac == nullptr)
                    throw cRuntimeError("SlRrc: configuredGrant needs an NrSlMacUe at '%s'",
                            par("slMacModule").stringValue());
                slMac->onConfiguredGrant(spec, type);
            }
        }
        else {
            if (check_and_cast<cValueMap *>(par("configuredGrant").objectValue())->containsKey("type"))
                throw cRuntimeError("SlRrc: a configured grant needs a serving cell (the UE is not attached)");
            EV << "SlRrc::resolvePool - node " << nodeId_ << ": not attached, falling back to the "
               << "local preconfig pool (out-of-coverage preconfiguration)" << endl;
        }
    }
    else if (check_and_cast<cValueMap *>(par("configuredGrant").objectValue())->containsKey("type")) {
        throw cRuntimeError("SlRrc: a configured grant requires poolSource=\"servingCell\"");
    }

    slBinder_->registerSlCarrier(GHz(preconfig_.carrierFrequencyGHz), preconfig_.numerologyIndex,
            preconfig_.subchannelSize, preconfig_.numSubchannels);
}

void SlRrc::handleMessage(cMessage *msg)
{
    if (msg->getArrivalGate() != nullptr && msg->getArrivalGate()->isName("srbIn")) {
        handlePc5RrcMessage(check_and_cast<inet::Packet *>(msg));
        return;
    }
    throw cRuntimeError("SlRrc: unexpected message '%s'", msg->getName());
}

void SlRrc::checkSlrbRlcMode(const FlowId& flow, const BearerRequest& req)
{
    // TR 38.885 5.4.2: a UM RLC entity serves broadcast, groupcast and unicast;
    // RLC AM is supported for sidelink unicast only. The transparent-mode bearer
    // carrying PC5-RRC is a unicast link bearer as well.
    if ((req.rlcType == AM || req.rlcType == TM) && req.slCastType != SL_UNICAST)
        throw cRuntimeError("SlRrc: RLC %s is only supported on sidelink unicast links "
                            "(TR 38.885 5.4.2), but bearer %d toward node %hu is %s",
                            req.rlcType == AM ? "AM" : "TM", (int)num(flow.drbId),
                            num(flow.destId), slCastTypeToA(req.slCastType).c_str());
}

void SlRrc::createSlOutgoingConnection(const FlowId& flow, const BearerRequest& req)
{
    Enter_Method_Silent("createSlOutgoingConnection()");
    checkSlrbRlcMode(flow, req);
    bearerManagement_->createSlOutgoingConnection(flow, req);
}

void SlRrc::createSlIncomingConnection(const FlowId& flow, const BearerRequest& req)
{
    Enter_Method_Silent("createSlIncomingConnection()");
    checkSlrbRlcMode(flow, req);
    bearerManagement_->createSlIncomingConnection(flow, req);
}

const SlrbConfigEntry& SlUnicastLink::findSlrbForPfi(int pfi) const
{
    const SlrbConfigEntry *def = nullptr;
    for (const auto& e : slrbs) {
        if (e.pfi == pfi)
            return e;
        if (e.isDefault)
            def = &e;
    }
    if (def == nullptr)
        def = &slrbs.front();  // no default marked: first entry catches all
    return *def;
}

const SlUnicastLink *SlRrc::findLink(MacNodeId peerId) const
{
    auto it = links_.find(peerId);
    return (it != links_.end()) ? &it->second : nullptr;
}

SlUnicastLink SlRrc::allocateLink(MacNodeId peerId, SlL2Id peerL2Id)
{
    // allocate the per-link SLRBs from the unicastSlrbDefaults templates
    // (D17): DRB ids from the dynamic range, fresh per link -- the D3 keying
    // is per (src, dst) pair, so ids may repeat across links but must be
    // unique within one
    SlUnicastLink link;
    link.peerId = peerId;
    unsigned short drb = SL_UNICAST_DRB_BASE;
    for (const auto& tmpl : preconfig_.unicastSlrbDefaults) {
        SlrbConfigEntry e = tmpl;
        e.castType = SL_UNICAST;
        e.dstL2Id = peerL2Id;
        if (drb >= SL_SRB_DRB_ID)
            throw cRuntimeError("SlRrc: too many unicastSlrbDefaults entries (link DRB ids exhausted at %hu)", drb);
        e.drbId = DrbId(drb++);
        link.slrbs.push_back(e);
    }
    return link;
}

const SlUnicastLink& SlRrc::establishLink(MacNodeId peerId)
{
    Enter_Method_Silent("establishLink()");

    auto it = links_.find(peerId);
    if (it != links_.end())
        return it->second;

    ASSERT(peerId != nodeId_);
    SlL2Id peerL2Id = slBinder_->getL2IdForNodeId(peerId);
    if (peerL2Id == SL_L2ID_NONE)
        throw cRuntimeError("SlRrc: cannot establish PC5 link to node %hu: not a registered SL UE", num(peerId));

    SlUnicastLink link = allocateLink(peerId, peerL2Id);

    if (!overTheAir_) {
        EV << "SlRrc::establishLink - node " << nodeId_ << ": PC5 unicast link to peer " << peerId
           << " (" << link.slrbs.size() << " SLRB(s)), genie handshake" << endl;

        const SlUnicastLink& stored = links_.emplace(peerId, std::move(link)).first->second;
        createLinkBearers(stored);

        // genie handshake: the peer adopts the same SLRB list and creates
        // its own symmetric chains (D18)
        SlRrc *peerRrc = slBinder_->getSlRrc(peerId);
        if (peerRrc == nullptr)
            throw cRuntimeError("SlRrc: no SlRrc registered for peer node %hu", num(peerId));
        peerRrc->onLinkRequest(nodeId_, stored.slrbs);
        return stored;
    }

    // over-the-air handshake (D23): the link parks in ESTABLISHING and the
    // proposed SLRBs travel as a real PC5-RRC message over the TM SL-SRB;
    // data chains come up when the response arrives
    link.state = SlUnicastLink::ESTABLISHING;
    const SlUnicastLink& stored = links_.emplace(peerId, std::move(link)).first->second;

    EV << "SlRrc::establishLink - node " << nodeId_ << ": PC5 unicast link to peer " << peerId
       << " ESTABLISHING (over-the-air handshake, " << stored.slrbs.size() << " SLRB(s) proposed)" << endl;

    auto request = inet::makeShared<SlLinkEstablishRequest>();
    request->setInitiatorId(nodeId_);
    request->setResponderId(peerId);
    int n = (int)stored.slrbs.size();
    request->setDrbIdsArraySize(n);
    request->setRlcTypesArraySize(n);
    request->setPfisArraySize(n);
    request->setPqisArraySize(n);
    request->setIsDefaultsArraySize(n);
    for (int i = 0; i < n; i++) {
        const SlrbConfigEntry& e = stored.slrbs[i];
        request->setDrbIds(i, num(e.drbId));
        request->setRlcTypes(i, e.rlcType);
        request->setPfis(i, e.pfi);
        request->setPqis(i, e.pqi);
        request->setIsDefaults(i, e.isDefault);
    }
    request->setChunkLength(inet::B(16 + 6 * n));
    sendPc5RrcMessage(peerId, request, "SlLinkEstablishRequest");

    return stored;
}

int SlRrc::ensureSrb(MacNodeId peerId)
{
    auto it = srbGates_.find(peerId);
    if (it != srbGates_.end())
        return it->second;

    // the SRB chains bootstrap via the genie mechanism at BOTH endpoints
    // (documented simplification: SRB0-3 as one pre-provisioned TM bearer);
    // only the DRB establishment handshake itself is over the air
    SlL2Id peerL2Id = slBinder_->getL2IdForNodeId(peerId);
    ASSERT(peerL2Id != SL_L2ID_NONE);

    FlowId out;
    out.direction = SL;
    out.sourceId = nodeId_;
    out.destId = peerId;
    out.drbId = DrbId(SL_SRB_DRB_ID);

    FlowId in = out.reversed();

    int idx = bearerManagement_->createSlSrbConnection(out, in, this);
    srbGates_[peerId] = idx;

    // mirror at the peer so it can receive (and answer on) the SRB
    SlRrc *peerRrc = slBinder_->getSlRrc(peerId);
    if (peerRrc == nullptr)
        throw cRuntimeError("SlRrc: no SlRrc registered for peer node %hu", num(peerId));
    peerRrc->ensureSrb(nodeId_);

    return idx;
}

void SlRrc::sendPc5RrcMessage(MacNodeId peerId, const inet::Ptr<inet::Chunk>& msg, const char *name)
{
    Enter_Method_Silent("sendPc5RrcMessage()");
    int gateIdx = ensureSrb(peerId);

    auto pkt = new inet::Packet(name);
    pkt->insertAtFront(msg);

    // minimal PDCP header for the TM entity's tracking contract (PC5-RRC
    // skips real PDCP; SRB integrity/security are out of SL-2 scope)
    auto pdcpHeader = inet::makeShared<LtePdcpHeader>();
    pdcpHeader->setSequenceNumber(pdcpSn_++);
    pkt->insertAtFront(pdcpHeader);

    auto lteInfo = pkt->addTag<FlowControlInfo>();
    lteInfo->setDirection(SL);
    lteInfo->setSourceId(nodeId_);
    lteInfo->setDestId(peerId);
    lteInfo->setDrbId(DrbId(SL_SRB_DRB_ID));

    EV << "SlRrc::sendPc5RrcMessage - node " << nodeId_ << ": " << name << " -> peer " << peerId
       << " over the SL-SRB" << endl;
    send(pkt, "srbOut", gateIdx);
}

void SlRrc::handlePc5RrcMessage(inet::Packet *pkt)
{
    pkt->popAtFront<LtePdcpHeader>();
    auto chunk = pkt->peekAtFront<SlPc5RrcMessage>();

    if (auto request = inet::dynamicPtrCast<const SlLinkEstablishRequest>(chunk)) {
        MacNodeId initiatorId = request->getInitiatorId();
        EV << "SlRrc::handlePc5RrcMessage - node " << nodeId_ << ": SlLinkEstablishRequest from "
           << initiatorId << " (" << request->getDrbIdsArraySize() << " SLRB(s))" << endl;

        if (!links_.count(initiatorId)) {
            // adopt the proposed SLRBs; from this side, the peer is the initiator
            SlL2Id initiatorL2Id = slBinder_->getL2IdForNodeId(initiatorId);
            SlUnicastLink link;
            link.peerId = initiatorId;
            link.state = SlUnicastLink::ESTABLISHED;
            for (int i = 0; i < (int)request->getDrbIdsArraySize(); i++) {
                SlrbConfigEntry e;
                e.castType = SL_UNICAST;
                e.dstL2Id = initiatorL2Id;
                e.drbId = DrbId(request->getDrbIds(i));
                e.rlcType = (LteRlcType)request->getRlcTypes(i);
                e.pfi = request->getPfis(i);
                e.pqi = request->getPqis(i);
                e.isDefault = request->isDefaults(i);
                link.slrbs.push_back(e);
            }
            const SlUnicastLink& stored = links_.emplace(initiatorId, std::move(link)).first->second;
            createLinkBearers(stored);
        }

        auto response = inet::makeShared<SlLinkEstablishResponse>();
        response->setInitiatorId(initiatorId);
        response->setResponderId(nodeId_);
        sendPc5RrcMessage(initiatorId, response, "SlLinkEstablishResponse");
    }
    else if (auto response = inet::dynamicPtrCast<const SlLinkEstablishResponse>(chunk)) {
        MacNodeId responderId = response->getResponderId();
        EV << "SlRrc::handlePc5RrcMessage - node " << nodeId_ << ": SlLinkEstablishResponse from "
           << responderId << endl;

        auto it = links_.find(responderId);
        if (it != links_.end() && it->second.state == SlUnicastLink::ESTABLISHING) {
            it->second.state = SlUnicastLink::ESTABLISHED;
            createLinkBearers(it->second);
            flushHeldPackets(responderId);
        }
    }
    else if (inet::dynamicPtrCast<const SlLinkRelease>(chunk)) {
        // no automatic trigger in SL-2 (no RLF/keepalive); accepted for
        // completeness of the message set
        EV << "SlRrc::handlePc5RrcMessage - node " << nodeId_ << ": SlLinkRelease (ignored)" << endl;
    }
    else
        throw cRuntimeError("SlRrc: unknown PC5-RRC message '%s'", pkt->getName());

    delete pkt;
}

void SlRrc::holdPacket(MacNodeId peerId, inet::Packet *pkt)
{
    Enter_Method_Silent("holdPacket()");
    take(pkt);
    heldPackets_[peerId].push_back(pkt);
    EV << "SlRrc::holdPacket - node " << nodeId_ << ": holding packet for peer " << peerId
       << " until the link is ESTABLISHED (" << heldPackets_[peerId].size() << " held)" << endl;
}

void SlRrc::flushHeldPackets(MacNodeId peerId)
{
    auto it = heldPackets_.find(peerId);
    if (it == heldPackets_.end())
        return;
    auto *slIp2Nic = check_and_cast<SlIp2Nic *>(getModuleByPath(par("ip2nicModule").stringValue()));
    EV << "SlRrc::flushHeldPackets - node " << nodeId_ << ": resuming " << it->second.size()
       << " held packet(s) for peer " << peerId << endl;
    for (auto *pkt : it->second)
        slIp2Nic->resumeHeldPacket(pkt);
    heldPackets_.erase(it);
}

void SlRrc::onLinkRequest(MacNodeId initiatorId, const std::vector<SlrbConfigEntry>& slrbs)
{
    Enter_Method_Silent("onLinkRequest()");

    if (links_.count(initiatorId))
        throw cRuntimeError("SlRrc: node %hu received a link request from %hu but already has that link",
                num(nodeId_), num(initiatorId));

    SlL2Id initiatorL2Id = slBinder_->getL2IdForNodeId(initiatorId);
    ASSERT(initiatorL2Id != SL_L2ID_NONE);

    // adopt the initiator's SLRB list; from this side, the peer is the initiator
    SlUnicastLink link;
    link.peerId = initiatorId;
    link.slrbs = slrbs;
    for (auto& e : link.slrbs)
        e.dstL2Id = initiatorL2Id;

    EV << "SlRrc::onLinkRequest - node " << nodeId_ << ": accepted PC5 unicast link from " << initiatorId << endl;

    const SlUnicastLink& stored = links_.emplace(initiatorId, std::move(link)).first->second;
    createLinkBearers(stored);
}

void SlRrc::createLinkBearers(const SlUnicastLink& link)
{
    for (const auto& e : link.slrbs) {
        // outgoing chain toward the peer: PDCP-TX -> RLC-TX -> MAC outgoing
        FlowId out;
        out.direction = SL;
        out.sourceId = nodeId_;
        out.destId = link.peerId;
        out.drbId = e.drbId;

        BearerRequest req;
        req.qosClass = BACKGROUND;
        req.rlcType = e.rlcType;
        req.slCastType = SL_UNICAST;
        req.slPqi = e.pqi;

        createSlOutgoingConnection(out, req);

        // incoming chain from the peer: MAC incoming -> RLC-RX -> PDCP-RX
        createSlIncomingConnection(out.reversed(), req);
    }
}

} // namespace simu5g
