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

using namespace omnetpp;

namespace simu5g {

void SdapDrbTable::addOrUpdateDrb(const DrbDesc& drb)
{
    // Insert or replace the entry (map nodes are stable, so index pointers stay valid)
    auto [it, inserted] = drbMap_.insert_or_assign(drb.key, drb);
    const DrbDesc *ptr = &it->second;
    MacNodeId ueNodeId = drb.getPeerId();

    // On an update, drop the QFI mappings of the replaced entry
    if (!inserted)
        for (auto qit = qfiToDrb_.begin(); qit != qfiToDrb_.end(); )
            qit = (qit->second == ptr) ? qfiToDrb_.erase(qit) : std::next(qit);

    // Reverse lookup: (nodeId, qfi) -> DrbDesc*
    for (Qfi qfi : ptr->mappedQfis)
        qfiToDrb_[{ueNodeId, qfi}] = ptr;
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

void SdapDrbTable::dump(std::ostream& os) const
{
    os << "=== SdapDrbTable dump (" << drbMap_.size() << " DRBs) ===" << std::endl;
    for (const auto& [key, ctx] : drbMap_) {
        os << "  " << key << ": " << ctx << std::endl;
    }
    os << "==============================" << std::endl;
}

} // namespace simu5g
