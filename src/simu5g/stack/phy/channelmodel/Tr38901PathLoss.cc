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

#include "simu5g/stack/phy/channelmodel/Tr38901PathLoss.h"

namespace simu5g {

Define_Module(Tr38901PathLoss);

void Tr38901PathLoss::handleMessage(cMessage *msg)
{
    throw cRuntimeError("unexpected message '%s': Tr38901PathLoss has no gates and schedules no self-messages", msg->getName());
}

} //namespace
