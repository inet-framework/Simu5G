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

#include "simu5g/stack/sidelink/ip2nic/SlPathPolicy.h"

#include <cstring>

namespace simu5g {

bool SlPathPolicy::parse(const char *name, Policy& out)
{
    if (!strcmp(name, "pc5IfPeer")) { out = PC5_IF_PEER; return true; }
    if (!strcmp(name, "uuIfServed")) { out = UU_IF_SERVED; return true; }
    if (!strcmp(name, "pc5Only")) { out = PC5_ONLY; return true; }
    if (!strcmp(name, "condition")) { out = CONDITION; return true; }
    return false;
}

SlPathPolicy::Decision SlPathPolicy::decideUnicast(Policy policy, bool peerSlCapable, bool served, bool conditionResult)
{
    switch (policy) {
        case PC5_IF_PEER:
            return peerSlCapable ? PATH_PC5 : PATH_UU;
        case UU_IF_SERVED:
            return (peerSlCapable && !served) ? PATH_PC5 : PATH_UU;
        case PC5_ONLY:
            return peerSlCapable ? PATH_PC5 : PATH_DENY;
        case CONDITION:
            return (peerSlCapable && conditionResult) ? PATH_PC5 : PATH_UU;
    }
    return PATH_UU;  // unreachable
}

} // namespace simu5g
