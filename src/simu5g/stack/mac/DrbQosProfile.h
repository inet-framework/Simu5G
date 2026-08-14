#ifndef __SIMU5G_DRBQOSPROFILE_H_
#define __SIMU5G_DRBQOSPROFILE_H_

#include "simu5g/common/LteCommon.h"

namespace simu5g {

// QoS profile of a data radio bearer's flows (5QI characteristics: GBR flag, packet
// delay budget, packet error rate, priority), consumed by QoS-aware MAC scheduling.
// Keyed externally by DrbKey (peer node, DRB id).
struct DrbQosProfile {
    bool gbr = false;
    double delayBudgetMs = 0;
    double packetErrorRate = 0;
    int priorityLevel = 0;
};

inline std::ostream& operator<<(std::ostream& os, const DrbQosProfile& e) {
    os << "gbr=" << e.gbr << " delay=" << e.delayBudgetMs
       << "ms per=" << e.packetErrorRate << " prio=" << e.priorityLevel;
    return os;
}

} // namespace simu5g

#endif
