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

#ifndef STACK_PHY_CHANNELMODEL_TR36814PATHLOSS_H_
#define STACK_PHY_CHANNELMODEL_TR36814PATHLOSS_H_

#include "simu5g/stack/phy/channelmodel/PathLossModule.h"
#include "simu5g/stack/phy/channelmodel/Tr36814PathLossModel.h"

namespace simu5g {

using namespace omnetpp;

/**
 * Mixin, INET's own shape: a NED submodule
 * that is also a Tr36814PathLossModel, so the strategy stays a plain,
 * stack-constructible object for the tests/unit/ harness while also being a
 * `like IPathLossModel` submodule of the medium. Carries no state and no
 * logic of its own -- everything is Tr36814PathLossModel's (the mixin's
 * cSimpleModule half, including handleMessage(), is PathLossModule's). In
 * particular it does not override cSimpleModule::initialize(): the
 * strategy's own "initialize" (owner_, scenario_, ...) is a same-named but
 * unrelated method of the unrelated PathLossModel base, called explicitly by
 * RadioMedium through a PathLossModel* pointer, never by the module
 * lifecycle.
 */
class Tr36814PathLoss : public PathLossModule, public Tr36814PathLossModel
{
};

} //namespace

#endif
