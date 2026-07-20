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
        slotGrid_ = SlSlotGrid(poolConfig_.getSlotDuration());

        // the mode-1 allocator (D29) over this cell's pool
        SlEnbScheduler::Config schedCfg;
        schedCfg.numSubchannels = poolConfig_.numSubchannels;
        schedCfg.subchannelSize = poolConfig_.subchannelSize;
        schedCfg.mcs = par("grantMcs");
        schedCfg.overheadSymbols = par("overheadSymbols");
        schedCfg.ueProcessingSlots = par("ueProcessingSlots");
        schedCfg.numOccasions = par("grantNumOccasions");
        schedCfg.occasionGapSlots = par("occasionGapSlots");
        schedCfg.schedulingHorizonSlots = par("schedulingHorizonSlots");
        slEnbScheduler_ = std::make_unique<SlEnbScheduler>(schedCfg);

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

SlEnbScheduler::GrantSpec SlGnbRrc::onSlBsr(MacNodeId ueId, int reportedBytes)
{
    Enter_Method_Silent("onSlBsr()");
    SlotIndex now = slotGrid_.slotIndexAt(simTime());
    SlEnbScheduler::GrantSpec spec = slEnbScheduler_->onSlBsr(ueId, reportedBytes, now);
    if (spec.isValid())
        EV << simTime() << " SlGnbRrc::onSlBsr - UE " << ueId << " reported " << reportedBytes
           << "B -> grant: first slot " << spec.firstSlot << ", " << spec.numOccasions
           << " occasion(s) every " << spec.occasionGapSlots << " slots, subchannels ["
           << spec.firstSubchannel << ".." << spec.firstSubchannel + spec.numSubchannels - 1
           << "], MCS " << spec.mcs << ", TBS " << spec.tbBytes << "B" << endl;
    else
        EV << simTime() << " SlGnbRrc::onSlBsr - UE " << ueId << " reported " << reportedBytes
           << "B -> no free resources within the horizon (UE will retry)" << endl;
    return spec;
}

} // namespace simu5g
