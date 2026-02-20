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

#ifndef STACK_SDAP_COMMON_QFICONTEXT_H_
#define STACK_SDAP_COMMON_QFICONTEXT_H_

#include <string>
#include <vector>
#include "simu5g/common/LteCommon.h"

namespace simu5g {

// Per-DRB configuration entry, built from the drbConfig JSON parameter.
struct DrbContext {
    int drbIndex = -1;              // global DRB instance index (= nrPdcp[]/nrRlc[] array index)
    MacNodeId ueNodeId = NODEID_NONE; // NODEID_NONE = "self" (UE side); resolved from "ue" module path on gNB
    int lcid = -1;                  // LCID = local DRB index within that UE's DRB set (auto-derived)
    std::vector<int> qfiList;       // QFIs mapped to this DRB
    bool gbr = false;
    double delayBudgetMs = 0;
    double packetErrorRate = 0;
    int priorityLevel = 0;
    std::string description;
};

inline std::ostream& operator<<(std::ostream& os, const DrbContext& ctx) {
    os << "ue=" << ctx.ueNodeId << " lcid=" << ctx.lcid << " qfi=[";
    for (int i = 0; i < (int)ctx.qfiList.size(); i++) {
        if (i) os << ",";
        os << ctx.qfiList[i];
    }
    os << "] gbr=" << ctx.gbr << " delay=" << ctx.delayBudgetMs
       << "ms prio=" << ctx.priorityLevel;
    if (!ctx.description.empty())
        os << " \"" << ctx.description << "\"";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const std::pair<MacNodeId,int>& p) {
    os << "(" << p.first << "," << p.second << ")";
    return os;
}

} // namespace simu5g

#endif /* STACK_SDAP_COMMON_QFICONTEXT_H_ */
