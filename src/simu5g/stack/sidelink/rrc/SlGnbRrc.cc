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

#include "simu5g/stack/sidelink/rrc/SlGnbRrc.h"

#include <inet/common/ModuleAccess.h>

#include "simu5g/stack/sidelink/common/SlBinder.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(SlGnbRrc);

void SlGnbRrc::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        cModule *node = inet::getContainingNode(this);
        cellId_ = MacNodeId(node->par("macNodeId").intValue());

        // a combined D2D+SL gNB is out of SL-3 scope (D27)
        if (node->hasPar("hasD2D") && node->par("hasD2D").boolValue())
            throw cRuntimeError("SlGnbRrc: a gNB with both hasD2D and sidelink support is not supported");

        auto *cfg = check_and_cast<cValueMap *>(par("slPoolConfig").objectValue());

        // only the pool section is meaningful on the gNB (the real SIB12
        // carries pools, not app bearers) - reject bearer-side keys loudly
        // instead of silently ignoring them
        for (const char *key : { "slrbConfig", "unicastSlrbDefaults", "pqiPriorityOverrides", "cbrConfig" })
            if (cfg->containsKey(key))
                throw cRuntimeError("SlGnbRrc: slPoolConfig must contain only the pool section "
                                    "('%s' is UE-local and belongs in the UE's preconfig)", key);

        poolConfig_.loadFromJson(cfg);

        SlBinder::getInstance()->registerSlGnbRrc(cellId_, this);

        EV << "SlGnbRrc::initialize - cell " << cellId_ << ", pool carrier "
           << poolConfig_.carrierFrequencyGHz << " GHz (mu=" << poolConfig_.numerologyIndex
           << "), " << poolConfig_.numSubchannels << " subchannels x "
           << poolConfig_.subchannelSize << " PRBs" << endl;
    }
}

void SlGnbRrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("SlGnbRrc: unexpected message '%s' (control-plane module, C++ interface only)", msg->getName());
}

void SlGnbRrc::registerSlUe(MacNodeId ueId)
{
    Enter_Method_Silent("registerSlUe()");
    slUes_.insert(ueId);
}

} // namespace simu5g
