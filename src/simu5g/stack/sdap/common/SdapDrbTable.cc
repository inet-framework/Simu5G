//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
///

#include "simu5g/stack/sdap/common/SdapDrbTable.h"
#include <set>
#include <omnetpp/cvaluearray.h>
#include <omnetpp/cvaluemap.h>

using namespace omnetpp;

namespace simu5g {

void SdapDrbTable::loadFromJson(const cValueArray *arr)
{
    for (int i = 0; i < (int)arr->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(arr->get(i).objectValue());

        DrbDesc ctx;
        DrbId drbId = DrbId(entry->get("drb").intValue());

        // "ue" field: numeric MacNodeId (gNB side); omitted on UE side
        MacNodeId ueNodeId = entry->containsKey("ue")
                ? MacNodeId(entry->get("ue").intValue())
                : NODEID_NONE;   // UE side: "self"
        ctx.key = DrbKey(ueNodeId, drbId);
        ctx.lcid = LogicalCid(num(drbId));

        // isDefault (optional; if not set, first DRB per nodeId becomes default)
        if (entry->containsKey("isDefault"))
            ctx.isDefault = entry->get("isDefault").boolValue();

        // qfiList
        const cValueArray *qfiArr = check_and_cast<const cValueArray *>(entry->get("qfiList").objectValue());
        for (int j = 0; j < (int)qfiArr->size(); j++)
            ctx.qfiList.push_back(Qfi(qfiArr->get(j).intValue()));

        // rlcType (optional, default UM)
        if (entry->containsKey("rlcType"))
            ctx.rlcType = aToRlcType(entry->get("rlcType").stdstringValue());

        // pduSessionType (optional, default IPv4)
        if (entry->containsKey("pduSessionType"))
            ctx.pduSessionType = aToPduSessionType(entry->get("pduSessionType").stdstringValue());

        // upperProtocol (optional, empty = derive from pduSessionType)
        if (entry->containsKey("upperProtocol"))
            ctx.upperProtocol = entry->get("upperProtocol").stdstringValue();

        drbMap_[ctx.key] = ctx;
    }

    // Build derived lookup tables (pointers into drbMap_, stable after insertion)
    // Also auto-assign isDefault to the first DRB per nodeId if none was explicitly marked
    std::set<MacNodeId> nodesWithExplicitDefault;
    for (auto& [key, ctx] : drbMap_) {
        if (ctx.isDefault)
            nodesWithExplicitDefault.insert(ctx.getPeerId());
    }

    std::set<MacNodeId> nodesWithAutoDefault;
    for (auto& [key, ctx] : drbMap_) {
        MacNodeId ueNodeId = ctx.getPeerId();
        // Auto-assign default if no explicit isDefault was set for this nodeId
        if (!nodesWithExplicitDefault.count(ueNodeId) &&
            !nodesWithAutoDefault.count(ueNodeId)) {
            ctx.isDefault = true;
            nodesWithAutoDefault.insert(ueNodeId);
        }

        // Build reverse lookup: (nodeId, qfi) -> DrbDesc*
        const DrbDesc *ptr = &ctx;
        for (Qfi qfi : ctx.qfiList)
            qfiToDrb_[{ueNodeId, qfi}] = ptr;

        // Build default DRB map
        if (ctx.isDefault) {
            defaultDrb_[ueNodeId] = ptr;
            if (ueNodeId == NODEID_NONE)
                ueDefaultDrb_ = ptr;
        }
    }
}

const DrbDesc *SdapDrbTable::getDrb(DrbKey key) const
{
    auto it = drbMap_.find(key);
    return it != drbMap_.end() ? &it->second : nullptr;
}

const DrbDesc *SdapDrbTable::getDrbForQfi(MacNodeId nodeId, Qfi qfi) const
{
    auto it = qfiToDrb_.find({nodeId, qfi});
    return it != qfiToDrb_.end() ? it->second : nullptr;
}

const DrbDesc *SdapDrbTable::getDefaultDrb(MacNodeId nodeId) const
{
    auto it = defaultDrb_.find(nodeId);
    return it != defaultDrb_.end() ? it->second : nullptr;
}

void SdapDrbTable::dump(std::ostream& os) const
{
    os << "=== SdapDrbTable dump (" << drbMap_.size() << " DRBs) ===" << std::endl;
    for (const auto& [key, ctx] : drbMap_) {
        os << "  " << key << ": " << ctx << std::endl;
    }
    os << "==============================" << std::endl;
}

} // namespace simu5g
