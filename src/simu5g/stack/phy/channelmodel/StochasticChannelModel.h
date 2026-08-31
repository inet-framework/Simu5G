//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_
#define STACK_PHY_CHANNELMODEL_STOCHASTICCHANNELMODEL_H_

// Compatibility alias for BackgroundCellChannelModel.cc's one remaining
// caller; remove this file together with BackgroundCellChannelModel.* .

#include "simu5g/stack/phy/channelmodel/Radio.h"

namespace simu5g {

using StochasticChannelModel = Radio;

} // namespace simu5g

#endif
