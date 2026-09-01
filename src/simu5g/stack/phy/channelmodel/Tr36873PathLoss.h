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

#ifndef STACK_PHY_CHANNELMODEL_TR36873PATHLOSS_H_
#define STACK_PHY_CHANNELMODEL_TR36873PATHLOSS_H_

#include <omnetpp.h>

#include "simu5g/stack/phy/channelmodel/Tr36873PathLossModel.h"

namespace simu5g {

using namespace omnetpp;

/**
 * Mixin, INET's own shape: a NED submodule
 * that is also a Tr36873PathLossModel. See Tr36814PathLoss for the full
 * rationale, shared by all three wrappers.
 */
class Tr36873PathLoss : public cSimpleModule, public Tr36873PathLossModel
{
  protected:
    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;
};

} //namespace

#endif
