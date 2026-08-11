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

#ifndef NRCHANNELMODEL_H_
#define NRCHANNELMODEL_H_

#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"

namespace simu5g {

/**
 * Near-empty subclass of LteRealisticChannelModel that overrides nothing in
 * C++: its NED type exists only to give the gNodeB and NR UE NICs' default
 * channel model a pathLossType default of "Tr36873" (see NrChannelModel.ned)
 * instead of the base class's own default of "Tr36814". Every behavior --
 * the propagation formulas, fading, interference, SINR assembly, the
 * reception decision -- comes from LteRealisticChannelModel, selected via
 * the pathLossType parameter.
 *
 * The name records the deployment this model is the default for (the gNodeB and
 * NR UE NICs), not a property of TR 36.873, which is itself an LTE study item.
 */
class NrChannelModel : public LteRealisticChannelModel
{
};

} //namespace

#endif /* NRCHANNELMODEL_H_ */

