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

#include "simu5g/stack/rrc/DrbTable.h"

#include <set>
#include <omnetpp/cvaluearray.h>
#include <omnetpp/cvaluemap.h>

namespace simu5g {

Define_Module(DrbTable);

void DrbTable::initialize()
{
    const cValueArray *arr = check_and_cast_nullable<const cValueArray *>(par("drbConfig").objectValue());
    const cValueMap *profiles = check_and_cast_nullable<const cValueMap *>(par("drbProfiles").objectValue());
    if (arr && arr->size() > 0) {
        loadConfig(arr, profiles);
        EV << "DrbTable: loaded " << configuredDrbs_.size() << " DRB entries from drbConfig" << endl;
        for (const auto& [key, drb] : configuredDrbs_)
            EV << "  " << key << ": " << drb << endl;
    }

    WATCH_MAP(drbs_);
    WATCH_MAP(configuredDrbs_);
}

void DrbTable::loadConfig(const cValueArray *arr, const cValueMap *profiles)
{
    for (int i = 0; i < (int)arr->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());

        // Resolve the entry's named profile, if any
        const cValueMap *profile = nullptr;
        if (entry->containsKey("profile")) {
            const char *name = entry->get("profile").stringValue();
            if (!profiles || !profiles->containsKey(name)) {
                std::string available;
                if (profiles)
                    for (const auto& [profileName, value] : profiles->getFields())
                        available += (available.empty() ? "" : ", ") + profileName;
                throw cRuntimeError("drbConfig entry %d references unknown profile '%s' (available: %s)",
                        i, name, available.empty() ? "none" : available.c_str());
            }
            profile = check_and_cast<const cValueMap *>(profiles->get(name).objectValue());
            for (const char *forbidden : { "drb", "ue", "profile" })
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

        DrbDesc drb;
        DrbId drbId = DrbId(entry->get("drb").intValue());

        // "ue" field: numeric MacNodeId (gNB side); omitted on UE side
        MacNodeId ueNodeId = entry->containsKey("ue")
                ? MacNodeId(entry->get("ue").intValue())
                : NODEID_NONE;   // UE side: "self"
        drb.key = DrbKey(ueNodeId, drbId);
        drb.lcid = LogicalCid(num(drbId));

        // isDefault (optional; if not set, first DRB per nodeId becomes default)
        if (const cValue *v = field("isDefault"))
            drb.isDefault = v->boolValue();

        // qfiList (optional; an entry without it does not take part in SDAP's
        // QFI-to-DRB mapping, e.g. it only carries the bearer's QoS profile)
        if (const cValue *v = field("qfiList")) {
            const cValueArray *qfiArr = check_and_cast<const cValueArray *>(v->objectValue());
            for (int j = 0; j < (int)qfiArr->size(); j++)
                drb.qfiList.push_back(Qfi(qfiArr->get(j).intValue()));
        }

        // QoS profile (all optional; any of them present = the bearer has a QoS
        // profile, pushed into the eNB MAC for QoS-aware scheduling)
        drb.hasQosProfile = field("gbr") || field("delayBudget") || field("per") || field("priority");
        if (const cValue *v = field("gbr"))
            drb.qos.gbr = v->boolValue();
        if (const cValue *v = field("delayBudget"))
            drb.qos.delayBudgetMs = v->doubleValue();
        if (const cValue *v = field("per"))
            drb.qos.packetErrorRate = v->doubleValue();
        if (const cValue *v = field("priority"))
            drb.qos.priorityLevel = v->intValue();

        // rlcType (optional; omitted = "RRC decides from qosClass", as for staticBearers)
        const cValue *rlcTypeValue = field("rlcType");
        drb.rlcType = rlcTypeValue ? aToRlcType(rlcTypeValue->stdstringValue()) : UNKNOWN_RLC_TYPE;

        // pduSessionType (optional, default IPv4)
        if (const cValue *v = field("pduSessionType"))
            drb.pduSessionType = aToPduSessionType(v->stdstringValue());

        // upperProtocol (optional, empty = derive from pduSessionType)
        if (const cValue *v = field("upperProtocol"))
            drb.upperProtocol = v->stdstringValue();

        configuredDrbs_[drb.key] = drb;
    }

    // Auto-assign isDefault to the first DRB per nodeId if none was explicitly marked
    std::set<MacNodeId> nodesWithDefault;
    for (auto& [key, drb] : configuredDrbs_)
        if (drb.isDefault)
            nodesWithDefault.insert(drb.getPeerId());
    for (auto& [key, drb] : configuredDrbs_) {
        if (!nodesWithDefault.count(drb.getPeerId())) {
            drb.isDefault = true;
            nodesWithDefault.insert(drb.getPeerId());
        }
    }
}

void DrbTable::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void DrbTable::refreshDisplay() const
{
    getDisplayString().setTagArg("t", 0, (std::to_string(drbs_.size()) + " DRBs").c_str());
}

const DrbDesc *DrbTable::findDrb(DrbKey key) const
{
    auto it = drbs_.find(key);
    return it != drbs_.end() ? &it->second : nullptr;
}

const DrbDesc *DrbTable::findConfiguredDrb(DrbKey key) const
{
    auto it = configuredDrbs_.find(key);
    return it != configuredDrbs_.end() ? &it->second : nullptr;
}

DrbDesc& DrbTable::getOrCreateDrb(DrbKey key)
{
    auto it = drbs_.find(key);
    if (it != drbs_.end())
        return it->second;
    DrbDesc& drb = drbs_[key];
    drb.key = key;
    return drb;
}

void DrbTable::removeDrb(DrbKey key)
{
    drbs_.erase(key);
}

void DrbTable::removeDrbsOfPeer(MacNodeId peerId)
{
    for (auto it = drbs_.begin(); it != drbs_.end(); ) {
        if (it->first.getNodeId() == peerId)
            it = drbs_.erase(it);
        else
            ++it;
    }
}

void DrbTable::removeAllDrbs()
{
    drbs_.clear();
}

} // namespace simu5g
