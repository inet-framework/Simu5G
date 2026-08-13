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

#include "simu5g/stack/sidelink/mac/SlHarqRxEntity.h"

namespace simu5g {

int SlHarqRxEntity::onReception(MacNodeId src, int procId, bool ndi)
{
    ProcessState& p = processes_[{src, procId}];
    if (!p.valid || p.ndi != ndi) {
        // new TB on this process
        p.valid = true;
        p.ndi = ndi;
        p.attempts = 0;
        p.delivered = false;
    }
    return ++p.attempts;
}

bool SlHarqRxEntity::isDelivered(MacNodeId src, int procId, bool ndi) const
{
    auto it = processes_.find({src, procId});
    return it != processes_.end() && it->second.valid && it->second.ndi == ndi && it->second.delivered;
}

void SlHarqRxEntity::markDelivered(MacNodeId src, int procId, bool ndi)
{
    ProcessState& p = processes_[{src, procId}];
    p.valid = true;
    p.ndi = ndi;
    p.delivered = true;
}

} // namespace simu5g
