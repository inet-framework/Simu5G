//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//
#include "simu5g/stack/mac/NrMacGnb.h"

#include "simu5g/stack/mac/packet/LteMacPdu.h"
#include "simu5g/stack/mac/packet/LteSchedulingGrant.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerEnbUl.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowSignals.h"

namespace simu5g {

Define_Module(NrMacGnb);

using namespace omnetpp;
using namespace inet;

NrMacGnb::NrMacGnb() : LteMacEnb()
{
    isNr_ = true;
}

} //namespace
