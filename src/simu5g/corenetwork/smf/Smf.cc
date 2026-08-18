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

#include <inet/common/PatternMatcher.h>
#include <inet/common/stlutils.h>

#include "simu5g/common/InitStages.h"
#include "simu5g/corenetwork/smf/Smf.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/rrc/Registration.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

Define_Module(Smf);

void Smf::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
    }
    else if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS) {
        // After INITSTAGE_SIMU5G_NODE_RELATIONSHIPS, so the UEs' serving nodes are known,
        // and before INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION, where the scheduler takes
        // the address of the QoS map that this fills through RRC.
        configureDrbs();
        parseTrafficClassRules();
    }
    else if (stage == inet::INITSTAGE_LAST) {
        establishStaticBearers();
    }
}

bool Smf::isDualConnectivityRequired(const FlowId& flow)
{
    MacNodeId sourceId = flow.sourceId;
    MacNodeId destId = flow.destId;

    // Part 1: Check if NodeB is in DC setup
    MacNodeId nodeB = (getNodeTypeById(sourceId) == UE) ? binder_->getServingNode(sourceId) : sourceId;
    ASSERT(nodeB != NODEID_NONE);

    MacNodeId secondaryNode = binder_->getSecondaryNode(nodeB);
    MacNodeId masterNode = binder_->getMasterNodeOrSelf(nodeB);
    bool nodeBInDC = (secondaryNode != NODEID_NONE) || (masterNode != nodeB);

    // Part 2: Check if UE is dual technology capable
    MacNodeId ue = getNodeTypeById(sourceId) == UE ? sourceId :
                   getNodeTypeById(destId) == UE ? destId :
                   NODEID_NONE;

    bool ueIsDualTech = false;  //TODO true? if a nodeB in DC setup sends multicast, can it use dual connectivity?
    if (ue != NODEID_NONE) {
        Registration *reg = check_and_cast<Registration*>(binder_->getRrcByNodeId(ue)->getSubmodule("registration"));
        ueIsDualTech = reg->isDualTechnology();
    }

    return nodeBInDC && ueIsDualTech;
}

DrbId Smf::assignDrbId(MacNodeId a, MacNodeId b)
{
    auto pair = std::minmax(a, b);
    auto& inUse = drbIdsInUse_[{pair.first, pair.second}];

    // Lowest free ID: identities released with their bearer are handed out again, which
    // is what keeps the space bounded for a UE that establishes and releases bearers
    // repeatedly (at every handover, say).
    unsigned short id = 1;
    while (inUse.count(DrbId(id)))
        id++;
    if (id > MAX_DRB_ID)
        throw cRuntimeError("Smf::assignDrbId - out of DRB identities for the node pair (%hu, %hu): "
                "all %d are in use", num(pair.first), num(pair.second), MAX_DRB_ID);

    inUse.insert(DrbId(id));
    return DrbId(id);
}

void Smf::reserveDrbId(MacNodeId a, MacNodeId b, DrbId drbId)
{
    auto pair = std::minmax(a, b);
    drbIdsInUse_[{pair.first, pair.second}].insert(drbId);
}

void Smf::releaseDrbId(MacNodeId a, MacNodeId b, DrbId drbId)
{
    auto pair = std::minmax(a, b);
    auto it = drbIdsInUse_.find({pair.first, pair.second});
    if (it != drbIdsInUse_.end() && it->second.erase(drbId) != 0)
        EV << "Smf::releaseDrbId - DRB " << drbId << " of the node pair (" << pair.first
           << ", " << pair.second << ") is free again" << endl;
}

void Smf::configureDrbs()
{
    // The node ids of each registered UE: one per stack, both naming the same UE and so
    // the same bearers. The LTE id, which every UE has, serves as the UE's identity here.
    std::map<cModule *, std::vector<MacNodeId>> ueNodeIds;
    for (const auto& [nodeId, info] : binder_->getNodeInfoMap())
        if (getNodeTypeById(nodeId) == UE && info.moduleRef != nullptr)
            ueNodeIds[info.moduleRef].push_back(nodeId);

    // UE module paths in the parameters are relative to the network
    std::string networkPrefix = std::string(getSystemModule()->getFullPath()) + ".";

    // The static bearers of each UE, collected before anything is pushed, so that the
    // default DRB of a UE can be settled while all of its bearers are in hand
    std::map<cModule *, std::map<DrbId, DrbDesc>> drbsOfUe;

    parseDrbDefinitions("staticDrbs", false, ueNodeIds, networkPrefix, drbsOfUe);
    parseDrbDefinitions("onDemandDrbs", true, ueNodeIds, networkPrefix, drbsOfUe);

    // A QFI is either mapped up front by a static definition or serves as an on-demand
    // selector; both claiming it would leave the on-demand entry permanently dead
    for (const AuthoredBearer& ab : authoredBearers_) {
        if (!ab.onDemand || ab.desc.bearerType != BEARER_5GC)
            continue;
        auto uit = drbsOfUe.find(ab.ueModule);
        if (uit == drbsOfUe.end())
            continue;
        for (const auto& [drbId, staticDrb] : uit->second)
            for (Qfi qfi : ab.desc.qfiList)
                if (contains(staticDrb.qfiList, qfi))
                    throw cRuntimeError("onDemandDrbs: QFI %d of UE '%s' is already mapped to static DRB %d",
                            (int)num(qfi), ab.ueModule->getFullPath().c_str(), (int)num(drbId));
    }

    for (auto& [ueModule, drbs] : drbsOfUe) {
        // The default DRB is where traffic with no QFI-to-DRB mapping (5gc) or no
        // matching packet filter (eps) goes; if the configuration does not name one
        // in either table, the UE's first static bearer takes the role
        bool onDemandDefault = false;
        for (const AuthoredBearer& ab : authoredBearers_)
            if (ab.onDemand && ab.ueModule == ueModule && ab.desc.isDefault)
                onDemandDefault = true;
        if (!onDemandDefault && std::none_of(drbs.begin(), drbs.end(), [](const auto& e) { return e.second.isDefault; }))
            drbs.begin()->second.isDefault = true;

        // The retained records are what establishment-time matching consults, so the
        // settled default is propagated into them
        for (AuthoredBearer& ab : authoredBearers_)
            if (!ab.onDemand && ab.ueModule == ueModule)
                ab.desc.isDefault = drbs.at(ab.desc.getDrbId()).isDefault;

        for (const auto& [drbId, drb] : drbs)
            pushDrbToRrcs(ueModule, drb);
    }
}

void Smf::parseDrbDefinitions(const char *paramName, bool onDemand,
        const std::map<cModule *, std::vector<MacNodeId>>& ueNodeIds, const std::string& networkPrefix,
        std::map<cModule *, std::map<DrbId, DrbDesc>>& drbsOfUe)
{
    const cValueArray *arr = check_and_cast_nullable<const cValueArray *>(par(paramName).objectValue());
    const cValueMap *profiles = check_and_cast_nullable<const cValueMap *>(par("drbProfiles").objectValue());
    if (arr == nullptr || arr->size() == 0)
        return;

    for (int i = 0; i < (int)arr->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());

        // Resolve the entry's named profile, if any. A profile describes what the bearer
        // is, so it must not carry the fields that say which UE it belongs to (ue, drb)
        // or which architecture and flows select it (bearerType, qfiList, filters,
        // isDefault).
        const cValueMap *profile = nullptr;
        if (entry->containsKey("profile")) {
            const char *name = entry->get("profile").stringValue();
            if (!profiles || !profiles->containsKey(name)) {
                std::string available;
                if (profiles)
                    for (const auto& [profileName, value] : profiles->getFields())
                        available += (available.empty() ? "" : ", ") + profileName;
                throw cRuntimeError("%s entry %d references unknown profile '%s' (available: %s)",
                        paramName, i, name, available.empty() ? "none" : available.c_str());
            }
            profile = check_and_cast<const cValueMap *>(profiles->get(name).objectValue());
            for (const char *forbidden : { "drb", "ue", "profile", "bearerType", "qfiList", "filters", "isDefault" })
                if (profile->containsKey(forbidden))
                    throw cRuntimeError("drbProfiles entry '%s' must not contain the '%s' field", name, forbidden);
        }

        // Field lookup: the entry's own value wins over the profile's
        auto field = [&](const char *key) -> const cValue * {
            if (entry->containsKey(key))
                return &entry->get(key);
            if (profile && profile->containsKey(key))
                return &profile->get(key);
            return nullptr;
        };

        // DRB id: static definitions pin it; an on-demand definition gets one assigned
        // when it first matches (see establishFromDefinition()/createOnDemandDrbForQfi())
        DrbDesc drb;
        DrbId drbId = DRBID_NONE;
        drb.key = DrbKey(NODEID_NONE, DRBID_NONE);
        if (!onDemand) {
            drbId = DrbId(entry->get("drb").intValue());
            drb.key = DrbKey(NODEID_NONE, drbId);
            drb.lcid = LogicalCid(num(drbId));
        }
        else if (entry->containsKey("drb"))
            throw cRuntimeError("%s entry %d: on-demand definitions do not name a \"drb\" id; one is assigned when the entry first matches", paramName, i);

        // bearerType (required): which architecture selects the bearer. Stated per
        // entry, never inferred; the receiving RRC checks it against its own stack
        // (see BearerManagement::configureDrb()).
        if (!entry->containsKey("bearerType"))
            throw cRuntimeError("%s entry %d: missing required field \"bearerType\" (\"eps\" or \"5gc\")", paramName, i);
        std::string bearerTypeStr = entry->get("bearerType").stdstringValue();
        drb.bearerType = aToBearerType(bearerTypeStr);
        if (drb.bearerType == UNKNOWN_BEARER_TYPE)
            throw cRuntimeError("%s entry %d: invalid bearerType '%s', must be \"eps\" or \"5gc\"", paramName, i, bearerTypeStr.c_str());

        // isDefault (optional; if no entry of a UE is marked, its first static one
        // becomes default). An on-demand "5gc" entry cannot be the default: the default
        // DRB is where unmapped QFIs go, so it must exist up front.
        if (entry->containsKey("isDefault"))
            drb.isDefault = entry->get("isDefault").boolValue();
        if (onDemand && drb.isDefault && drb.bearerType == BEARER_5GC)
            throw cRuntimeError("%s entry %d: an on-demand \"5gc\" definition cannot be the default DRB", paramName, i);

        // qfiList (5gc only, optional; an entry without it does not take part in SDAP's
        // QFI-to-DRB mapping, e.g. it only carries the bearer's QoS profile)
        if (entry->containsKey("qfiList")) {
            if (drb.bearerType != BEARER_5GC)
                throw cRuntimeError("%s entry %d: \"qfiList\" is a \"5gc\" selector, not valid on a \"%s\" bearer", paramName, i, bearerTypeStr.c_str());
            const cValueArray *qfiArr = check_and_cast<const cValueArray *>(entry->get("qfiList").objectValue());
            for (int j = 0; j < (int)qfiArr->size(); j++)
                drb.qfiList.push_back(Qfi(qfiArr->get(j).intValue()));
        }

        // filters (eps only, optional; the packet filters that select this bearer --
        // an entry without them can still be the default bearer or carry only a QoS
        // profile). Compiled below, once per matched UE, so a syntax error fails at
        // setup, not on the first packet.
        if (entry->containsKey("filters")) {
            if (drb.bearerType != BEARER_EPS)
                throw cRuntimeError("%s entry %d: \"filters\" is an \"eps\" selector, not valid on a \"%s\" bearer", paramName, i, bearerTypeStr.c_str());
            const cValueArray *fArr = check_and_cast<const cValueArray *>(entry->get("filters").objectValue());
            for (int j = 0; j < (int)fArr->size(); j++)
                drb.filters.push_back(fArr->get(j).stdstringValue());
        }

        // QoS profile (all optional; any of them present = the bearer has a QoS profile,
        // which RRC pushes into the eNB/gNB MAC for QoS-aware scheduling)
        drb.hasQosProfile = field("gbr") || field("delayBudget") || field("per") || field("priority");
        if (const cValue *v = field("gbr"))
            drb.qos.gbr = v->boolValue();
        if (const cValue *v = field("delayBudget"))
            drb.qos.delayBudgetMs = v->doubleValue();
        if (const cValue *v = field("per"))
            drb.qos.packetErrorRate = v->doubleValue();
        if (const cValue *v = field("priority"))
            drb.qos.priorityLevel = v->intValue();

        // rlcType (optional; omitted = "RRC decides from the QoS class", as for staticBearers)
        drb.rlcType = UNKNOWN_RLC_TYPE;
        if (const cValue *v = field("rlcType")) {
            std::string rlcTypeStr = v->stdstringValue();
            drb.rlcType = aToRlcType(rlcTypeStr);
            if (drb.rlcType == UNKNOWN_RLC_TYPE)
                throw cRuntimeError("%s entry %d: invalid rlcType '%s', must be \"TM\", \"UM\" or \"AM\"",
                        paramName, i, rlcTypeStr.c_str());
        }

        // qosClass (eps only, optional; the bearer's traffic class, which establishment
        // turns into the logical channel group; omitted = conversational. 5gc
        // establishment does not consume it, so authoring it there is rejected
        // rather than silently ignored.)
        if (const cValue *v = field("qosClass")) {
            if (drb.bearerType != BEARER_EPS)
                throw cRuntimeError("%s entry %d: \"qosClass\" is only supported on \"eps\" bearers", paramName, i);
            std::string qosClassStr = v->stdstringValue();
            drb.lcg = aToLteTrafficClass(qosClassStr);
            if (drb.lcg == UNKNOWN_TRAFFIC_TYPE)
                throw cRuntimeError("%s entry %d: invalid qosClass '%s', must be \"CONVERSATIONAL\", \"STREAMING\", \"INTERACTIVE\" or \"BACKGROUND\"",
                        paramName, i, qosClassStr.c_str());
        }

        // pduSessionType (optional, default IPv4) and upperProtocol (optional, empty =
        // derive from pduSessionType)
        if (const cValue *v = field("pduSessionType"))
            drb.pduSessionType = aToPduSessionType(v->stdstringValue());
        if (const cValue *v = field("upperProtocol"))
            drb.upperProtocol = v->stdstringValue();

        // The entry names its UE by module path (patterns allowed), which is how the
        // configuration follows the UE instead of naming an allocation-order-dependent
        // id; an on-demand entry may omit it to cover every UE
        const char *uePattern = entry->containsKey("ue") ? entry->get("ue").stringValue() : nullptr;
        if (uePattern == nullptr) {
            if (!onDemand)
                throw cRuntimeError("%s entry %d: missing required field \"ue\"", paramName, i);
            uePattern = "**";
        }
        inet::PatternMatcher matcher(uePattern, true, true, true);
        int numMatched = 0;
        for (const auto& [ueModule, nodeIds] : ueNodeIds) {
            std::string path = ueModule->getFullPath();
            if (path.compare(0, networkPrefix.size(), networkPrefix) == 0)
                path.erase(0, networkPrefix.size());
            if (!matcher.matches(path.c_str()))
                continue;
            numMatched++;
            if (!onDemand && !drbsOfUe[ueModule].insert({drbId, drb}).second)
                throw cRuntimeError("%s entry %d: DRB %d of UE '%s' is already configured by an earlier entry",
                        paramName, i, (int)num(drbId), path.c_str());

            // Retain the definition for establishment-time matching, its filters
            // compiled; one record per (entry x UE), so an on-demand definition
            // materializes separately per UE
            AuthoredBearer ab;
            ab.ueModule = ueModule;
            ab.desc = drb;
            ab.onDemand = onDemand;
            for (const std::string& spec : drb.filters) {
                auto filter = std::make_unique<inet::PacketFilter>();
                configurePacketFilter(*filter, spec.c_str());
                ab.filters.push_back(std::move(filter));
            }
            authoredBearers_.push_back(std::move(ab));
        }
        if (numMatched == 0)
            throw cRuntimeError("%s entry %d: its \"ue\" pattern '%s' matches no registered UE", paramName, i, uePattern);
    }
}

void Smf::pushDrbToRrcs(cModule *ueModule, const DrbDesc& drb)
{
    // node ids of the UE module, one per stack (see configureDrbs())
    std::vector<MacNodeId> nodeIds;
    for (const auto& [nodeId, info] : binder_->getNodeInfoMap())
        if (getNodeTypeById(nodeId) == UE && info.moduleRef == ueModule)
            nodeIds.push_back(nodeId);
    ASSERT(!nodeIds.empty());

    DrbId drbId = drb.getDrbId();

    // The UE keys its bearers by "my serving node" (NODEID_NONE), its serving
    // node by the UE. A dual-stack UE has one bearer per stack id, and the
    // serving node of each stack is told about the one that is its own.
    auto *ueRrc = check_and_cast<BearerManagement *>(binder_->getRrcByNodeId(nodeIds.front())->getSubmodule("bearerManagement"));
    DrbDesc ueDrb = drb;
    ueDrb.key = DrbKey(NODEID_NONE, drbId);
    ueRrc->configureDrb(ueDrb);

    for (MacNodeId ueId : nodeIds) {
        MacNodeId servingNodeId = binder_->getServingNode(ueId);
        if (servingNodeId == NODEID_NONE)
            continue;   // this stack is not attached to a cell
        DrbDesc enbDrb = drb;
        enbDrb.key = DrbKey(ueId, drbId);
        auto *enbRrc = check_and_cast<BearerManagement *>(binder_->getRrcByNodeId(servingNodeId)->getSubmodule("bearerManagement"));
        enbRrc->configureDrb(enbDrb);

        // The configuration names the bearer, so its id is taken out of the pool
        // that assignDrbId() hands out to bearers that are not configured here
        reserveDrbId(ueId, servingNodeId, drbId);
    }
}

void Smf::establishStaticBearers()
{
    auto *entries = check_and_cast<cValueArray *>(par("staticBearers").objectValue());
    for (int i = 0; i < (int)entries->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(entries->get(i).objectValue());

        // resolve the UE module to its registered node id(s) -- one per stack
        const char *uePath = entry->get("ue").stringValue();
        cModule *ueModule = getSimulation()->getSystemModule()->findModuleByPath(uePath);
        if (ueModule == nullptr)
            throw cRuntimeError("staticBearers: no module at path '%s'", uePath);
        MacNodeId lteUeId = NODEID_NONE, nrUeId = NODEID_NONE;
        for (const auto& [nodeId, info] : binder_->getNodeInfoMap())
            if (info.moduleRef == ueModule) {
                if (getNodeTypeById(nodeId) != UE)
                    throw cRuntimeError("staticBearers: module '%s' is not a UE", uePath);
                (num(nodeId) >= NR_UE_MIN_ID ? nrUeId : lteUeId) = nodeId;
            }
        if (lteUeId == NODEID_NONE && nrUeId == NODEID_NONE)
            throw cRuntimeError("staticBearers: module '%s' is not a registered UE", uePath);

        // select the UE's stack: the explicit technology field, or the same default
        // that packet-triggered establishment uses (see Ip2Nic::assignBearer): the
        // technology-neutral LTE id when the serving nodes form a DC setup (so that
        // establishDataConnection() splits the bearer into legs), the NR id otherwise
        MacNodeId ueId;
        if (entry->containsKey("technology")) {
            std::string tech = entry->get("technology").stdstringValue();
            if (tech != "LTE" && tech != "NR")
                throw cRuntimeError("staticBearers: invalid technology '%s' for UE '%s', must be \"LTE\" or \"NR\"", tech.c_str(), uePath);
            ueId = (tech == "NR") ? nrUeId : lteUeId;
            if (ueId == NODEID_NONE)
                throw cRuntimeError("staticBearers: UE '%s' has no %s stack", uePath, tech.c_str());
        }
        else {
            bool lteAttached = lteUeId != NODEID_NONE && binder_->getServingNode(lteUeId) != NODEID_NONE;
            bool nrAttached = nrUeId != NODEID_NONE && binder_->getServingNode(nrUeId) != NODEID_NONE;
            if (!lteAttached && !nrAttached)
                throw cRuntimeError("staticBearers: UE '%s' is not attached to any cell", uePath);
            MacNodeId lteNodeB = lteAttached ? binder_->getServingNode(lteUeId) : NODEID_NONE;
            bool dcSetup = lteNodeB != NODEID_NONE &&
                    (binder_->getSecondaryNode(lteNodeB) != NODEID_NONE || binder_->getMasterNodeOrSelf(lteNodeB) != lteNodeB);
            ueId = (lteAttached && nrAttached && dcSetup) ? lteUeId :
                   nrAttached ? nrUeId : lteUeId;
        }

        MacNodeId servingNodeId = binder_->getServingNode(ueId);
        if (servingNodeId == NODEID_NONE)
            throw cRuntimeError("staticBearers: UE '%s' (nodeId=%hu) is not attached to a cell", uePath, num(ueId));

        // DRB id: explicit (establishDataConnection() reserves it, so on-demand
        // establishment cannot hand out the same id later), or left unset for it to
        // assign the lowest free one
        DrbId drbId = DRBID_NONE;
        if (entry->containsKey("drb"))
            drbId = DrbId(entry->get("drb").intValue());

        LteTrafficClass qosClass = CONVERSATIONAL;
        if (entry->containsKey("qosClass")) {
            std::string qosClassStr = entry->get("qosClass").stdstringValue();
            qosClass = aToLteTrafficClass(qosClassStr);
            if (qosClass == UNKNOWN_TRAFFIC_TYPE)
                throw cRuntimeError("staticBearers: invalid qosClass '%s' for UE '%s'", qosClassStr.c_str(), uePath);
        }
        LteRlcType rlcType = UNKNOWN_RLC_TYPE;  // = RRC decides from qosClass
        if (entry->containsKey("rlcType")) {
            std::string rlcTypeStr = entry->get("rlcType").stdstringValue();
            rlcType = aToRlcType(rlcTypeStr);
            if (rlcType == UNKNOWN_RLC_TYPE)
                throw cRuntimeError("staticBearers: invalid rlcType '%s' for UE '%s'", rlcTypeStr.c_str(), uePath);
        }

        FlowId flow;
        flow.sourceId = ueId;
        flow.destId = servingNodeId;
        flow.direction = UL;
        flow.drbId = drbId;

        EV << "Smf::establishStaticBearers - establishing a bearer for UE '" << uePath
           << "' (nodeId=" << ueId << ") towards serving node " << servingNodeId << endl;
        establishDataConnection(flow, BearerRequest{qosClass, rlcType});
    }
}

DrbId Smf::establishOnDemandBearer(const FlowId& flow, const FlowBindingKey& key, const inet::Packet *pkt)
{
    Enter_Method_Silent("establishOnDemandBearer");

    // The requester brings identity only; the bearer's properties are authored here.
    // Authored definitions describe infrastructure bearers, so multicast and D2D
    // flows go straight to the fallback below.
    if (flow.multicastGroupId == NODEID_NONE && flow.d2dTxPeerId == NODEID_NONE && flow.d2dRxPeerId == NODEID_NONE
            && !authoredBearers_.empty()) {
        MacNodeId ueId = getNodeTypeById(flow.sourceId) == UE ? flow.sourceId : flow.destId;
        cModule *ueModule = binder_->getNodeModule(ueId);

        if (ueModule != nullptr) {
            // First matching definition wins, in table order (staticDrbs records are
            // retained ahead of onDemandDrbs ones); the default eps entry catches the
            // flows no filter matched.
            AuthoredBearer *defaultDef = nullptr;
            for (auto& ab : authoredBearers_) {
                if (ab.ueModule != ueModule || ab.desc.bearerType != BEARER_EPS)
                    continue;
                for (auto& filter : ab.filters)
                    if (filter->matches(pkt))
                        return establishFromDefinition(ab, flow, key);
                if (ab.desc.isDefault && defaultDef == nullptr)
                    defaultDef = &ab;
            }
            if (defaultDef != nullptr)
                return establishFromDefinition(*defaultDef, flow, key);
        }
    }

    // No definition covers the flow: author the bearer from the packet
    return establishDataConnection(flow, BearerRequest{classifyTrafficClass(pkt), UNKNOWN_RLC_TYPE, key});
}

DrbId Smf::establishFromDefinition(AuthoredBearer& ab, const FlowId& flowIn, const FlowBindingKey& key)
{
    FlowId flow = flowIn;
    if (ab.desc.getDrbId() == DRBID_NONE) {
        // First match of an on-demand definition: assign its id within the flow's node
        // pair and deliver it to the RRCs, as configureDrbs() does for static
        // definitions -- from here on the bearer has an authored identity, which later
        // flows matching the same definition join.
        DrbId drbId = assignDrbId(flow.sourceId, flow.destId);
        ab.desc.key = DrbKey(NODEID_NONE, drbId);
        ab.desc.lcid = LogicalCid(num(drbId));
        EV << "Smf::establishFromDefinition - on-demand definition materialized as DRB " << drbId
           << " for UE " << ab.ueModule->getFullPath() << endl;
        pushDrbToRrcs(ab.ueModule, ab.desc);
    }
    flow.drbId = ab.desc.getDrbId();
    return establishDataConnection(flow, BearerRequest{ab.desc.lcg, ab.desc.rlcType, key});
}

DrbId Smf::createOnDemandDrbForQfi(MacNodeId ueNodeId, Qfi qfi)
{
    Enter_Method_Silent("createOnDemandDrbForQfi");

    cModule *ueModule = binder_->getNodeModule(ueNodeId);
    if (ueModule == nullptr)
        return DRBID_NONE;

    for (auto& ab : authoredBearers_) {
        if (!ab.onDemand || ab.ueModule != ueModule || ab.desc.bearerType != BEARER_5GC)
            continue;
        if (!contains(ab.desc.qfiList, qfi))
            continue;
        if (ab.desc.getDrbId() == DRBID_NONE) {
            MacNodeId servingNodeId = binder_->getServingNode(ueNodeId);
            if (servingNodeId == NODEID_NONE)
                return DRBID_NONE;   // not attached, nowhere to create the bearer
            DrbId drbId = assignDrbId(ueNodeId, servingNodeId);
            ab.desc.key = DrbKey(NODEID_NONE, drbId);
            ab.desc.lcid = LogicalCid(num(drbId));
            EV << "Smf::createOnDemandDrbForQfi - QFI " << (int)num(qfi) << " gets on-demand DRB " << drbId
               << " at UE " << ueModule->getFullPath() << endl;
            pushDrbToRrcs(ab.ueModule, ab.desc);
        }
        return ab.desc.getDrbId();
    }
    return DRBID_NONE;
}

void Smf::parseTrafficClassRules()
{
    const cValueArray *arr = check_and_cast<const cValueArray *>(par("trafficClassRules").objectValue());
    for (int i = 0; i < (int)arr->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());
        for (const auto& [key, value] : entry->getFields())
            if (key != "filter" && key != "qosClass")
                throw cRuntimeError("trafficClassRules entry %d: unknown field '%s'", i, key.c_str());

        TrafficClassRule rule;
        if (!entry->containsKey("qosClass"))
            throw cRuntimeError("trafficClassRules entry %d: missing required field \"qosClass\"", i);
        std::string qosClassStr = entry->get("qosClass").stdstringValue();
        rule.qosClass = aToLteTrafficClass(qosClassStr);
        if (rule.qosClass == UNKNOWN_TRAFFIC_TYPE)
            throw cRuntimeError("trafficClassRules entry %d: invalid qosClass '%s', must be \"CONVERSATIONAL\", \"STREAMING\", \"INTERACTIVE\" or \"BACKGROUND\"",
                    i, qosClassStr.c_str());
        if (entry->containsKey("filter")) {
            rule.filter = std::make_unique<inet::PacketFilter>();
            configurePacketFilter(*rule.filter, entry->get("filter").stringValue());
        }
        trafficClassRules_.push_back(std::move(rule));
    }
}

LteTrafficClass Smf::classifyTrafficClass(const inet::Packet *pkt)
{
    for (const TrafficClassRule& rule : trafficClassRules_)
        if (rule.filter == nullptr || rule.filter->matches(pkt))
            return rule.qosClass;
    return BACKGROUND;
}

DrbId Smf::establishDataConnection(const FlowId& flowIn, const BearerRequest& req)
{
    // Assign the bearer's DRB id unless the requester brought one (SDAP and the
    // staticBearers entries name their bearers explicitly). IDs are unique per node
    // pair; for multicast the "pair" is (sender, group), there being no single peer.
    FlowId flow = flowIn;
    MacNodeId peerId = (flow.multicastGroupId != NODEID_NONE) ? flow.multicastGroupId : flow.destId;
    if (flow.drbId == DRBID_NONE) {
        flow.drbId = assignDrbId(flow.sourceId, peerId);
        EV << "Smf::establishDataConnection - new DRB ID assigned: " << flow.drbId << endl;
    }
    else
        reserveDrbId(flow.sourceId, peerId, flow.drbId);   // named by the requester; keep assignDrbId off it

    bool dualConnected = isDualConnectivityRequired(flow);
    if (!dualConnected) {
        createConnection(flow, req, true);
    }
    else {
        MacNodeId sourceId = flow.sourceId;
        MacNodeId destId = flow.destId;
        bool isMulticast = flow.multicastGroupId != NODEID_NONE;

        // Get UE registration if any endpoint is UE
        Registration *ueReg = (getNodeTypeById(sourceId) == UE) ? check_and_cast<Registration*>(binder_->getRrcByNodeId(sourceId)->getSubmodule("registration")) :
                     (!isMulticast && getNodeTypeById(destId) == UE) ? check_and_cast<Registration*>(binder_->getRrcByNodeId(destId)->getSubmodule("registration")) :
                     nullptr;

        //TODO assert that master is LTE, and secondary is NT;   alternatively, choose the UE nodeId that matches the technology of the NODEB

        // LTE Connection (Master)
        FlowId lteFlow = flow;
        lteFlow.sourceId = getNodeTypeById(sourceId) == UE ?
                            ueReg->getLteNodeId() :
                            binder_->getMasterNodeOrSelf(sourceId);
        if (!isMulticast) {  // Only set destId for unicast
            lteFlow.destId = getNodeTypeById(destId) == UE ?
                              ueReg->getLteNodeId() :
                              binder_->getMasterNodeOrSelf(destId);
        }
        createConnection(lteFlow, req, true);

        // NR Connection (Secondary)
        FlowId nrFlow = flow;
        nrFlow.sourceId = getNodeTypeById(sourceId) == UE ?
                           ueReg->getNrNodeId() :
                           binder_->getSecondaryNode(binder_->getMasterNodeOrSelf(sourceId));
        if (!isMulticast) {  // Only set destId for unicast
            nrFlow.destId = getNodeTypeById(destId) == UE ?
                             ueReg->getNrNodeId() :
                             binder_->getSecondaryNode(binder_->getMasterNodeOrSelf(destId));
        }
        createConnection(nrFlow, req, false);
    }
    return flow.drbId;
}

void Smf::createConnection(const FlowId& flow, const BearerRequest& req, bool withPdcp)
{
    MacNodeId sourceId = flow.sourceId;
    MacNodeId destId = flow.destId;
    MacNodeId groupId = flow.multicastGroupId;

    EV << "Smf::establishDataConnection - establishing connection from sourceId=" << sourceId
       << " to destId=" << destId << " groupId=" << groupId << endl;

    bool sourceIsEnb = getNodeTypeById(sourceId) == NODEB;
    bool destIsEnb = getNodeTypeById(destId) == NODEB;
    ASSERT(!sourceIsEnb || !destIsEnb);  // they cannot be both NodeBs

    bool sourceWithPdcp = getNodeTypeById(sourceId)==UE || withPdcp;
    createOutgoingConnectionOnNode(sourceId, flow, req, sourceWithPdcp);

    if (groupId == NODEID_NONE) {
        bool destWithPdcp = getNodeTypeById(destId)==UE || withPdcp;
        createIncomingConnectionOnNode(destId, flow, req, destWithPdcp);

        // A DRB is bidirectional (TS 38.331): create the reverse leg of the bearer
        // at both endpoints as well, so reverse traffic -- user data or RLC-AM
        // STATUS PDUs -- finds its entities in place instead of establishing a
        // separate unidirectional bearer.
        // The reverse leg is the same bearer with the same configuration, seen from the
        // other end -- including the flow key, which the peer binds as IT sees the flow
        // (addresses swapped, direction reversed).
        FlowId revFlow = flow.reversed();
        BearerRequest revReq = req;
        if (revReq.flowBindingKey.has_value())
            revReq.flowBindingKey = revReq.flowBindingKey->reversed();
        createOutgoingConnectionOnNode(destId, revFlow, revReq, destWithPdcp);
        createIncomingConnectionOnNode(sourceId, revFlow, revReq, sourceWithPdcp);
    }
    else {
        // Multicast bearers stay unidirectional: TX at the sender, RX at the members
        for (auto& [nodeId,_] : binder_->getNodeInfoMap())  //TODO use lte ones if LTE in DC setup, and NR ones if NR in DC setup
            if (nodeId != sourceId && binder_->isInMulticastGroup(nodeId, groupId))
                createIncomingConnectionOnNode(nodeId, flow, req, getNodeTypeById(nodeId)==UE || withPdcp);
    }
}


void Smf::createIncomingConnectionOnNode(MacNodeId nodeId, const FlowId& flow, const BearerRequest& req, bool withPdcp)
{
    BearerManagement *bm = check_and_cast<BearerManagement*>(binder_->getRrcByNodeId(nodeId)->getSubmodule("bearerManagement"));
    bm->createIncomingConnection(flow, req, withPdcp);
}

void Smf::createOutgoingConnectionOnNode(MacNodeId nodeId, const FlowId& flow, const BearerRequest& req, bool withPdcp)
{
    BearerManagement *bm = check_and_cast<BearerManagement*>(binder_->getRrcByNodeId(nodeId)->getSubmodule("bearerManagement"));
    bm->createOutgoingConnection(flow, req, withPdcp);
}

} //namespace
