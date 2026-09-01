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

#include "simu5g/stack/phy/channelmodel/PathLossModule.h"

namespace simu5g {

void PathLossModule::handleMessage(cMessage *msg)
{
    // getComponentType()->getName(): the NED type's own unqualified name
    // (e.g. "Tr36814PathLoss"), so the message still names the concrete
    // study, the same as when each mixin threw this itself.
    throw cRuntimeError("unexpected message '%s': %s has no gates and schedules no self-messages",
            msg->getName(), getComponentType()->getName());
}

} //namespace
