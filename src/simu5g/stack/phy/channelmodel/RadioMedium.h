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

#ifndef STACK_PHY_CHANNELMODEL_RADIOMEDIUM_H_
#define STACK_PHY_CHANNELMODEL_RADIOMEDIUM_H_

#include <omnetpp.h>

namespace simu5g {

using namespace omnetpp;

/**
 * The central, network-level module that models the shared physical radio
 * medium of a cellular network. It is the single owner of the physical facts
 * and channel effects of every radio link, shared by every carrier and every
 * radio endpoint that registers with it.
 */
class RadioMedium : public cSimpleModule
{
  public:
    void initialize() override {}

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;
};

} //namespace

#endif
