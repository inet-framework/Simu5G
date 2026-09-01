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

#ifndef STACK_PHY_CHANNELMODEL_PATHLOSSMODULE_H_
#define STACK_PHY_CHANNELMODEL_PATHLOSSMODULE_H_

#include <omnetpp.h>

namespace simu5g {

using namespace omnetpp;

/**
 * The cSimpleModule half shared by the three path-loss mixins
 * (Tr36814PathLoss, Tr36873PathLoss, Tr38901PathLoss -- see any one of them
 * for the mixin's own rationale): none of the three has gates or schedules
 * self-messages, so handleMessage() rejecting one is the only module
 * lifecycle behavior any of them needs, and it does not depend on which
 * concrete study the leaf class is.
 */
class PathLossModule : public cSimpleModule
{
  protected:
    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;
};

} //namespace

#endif
