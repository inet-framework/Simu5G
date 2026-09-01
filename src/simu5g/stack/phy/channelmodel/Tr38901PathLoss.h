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

#ifndef STACK_PHY_CHANNELMODEL_TR38901PATHLOSS_H_
#define STACK_PHY_CHANNELMODEL_TR38901PATHLOSS_H_

#include "simu5g/stack/phy/channelmodel/PathLossModule.h"
#include "simu5g/stack/phy/channelmodel/Tr38901PathLossModel.h"

namespace simu5g {

using namespace omnetpp;

/**
 * Mixin, INET's own shape: a NED submodule
 * that is also a Tr38901PathLossModel. See Tr36814PathLoss for the full
 * rationale, shared by all three wrappers. Its own
 * "useBuildingPenetrationHighLossModel" NED parameter is not read here --
 * RadioMedium reads it and calls setUseBuildingPenetrationHighLossModel()
 * explicitly, the same way it already does for a freshly constructed
 * strategy, so the module itself carries no initialize() override.
 */
class Tr38901PathLoss : public PathLossModule, public Tr38901PathLossModel
{
};

} //namespace

#endif
