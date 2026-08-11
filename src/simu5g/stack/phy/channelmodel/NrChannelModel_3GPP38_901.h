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

#ifndef NRCHANNELMODEL_3GPP38_901_H_
#define NRCHANNELMODEL_3GPP38_901_H_

#include "simu5g/stack/phy/channelmodel/NrChannelModel.h"

namespace simu5g {

/**
 * Near-empty subclass of NrChannelModel that overrides nothing in C++: its
 * NED type exists only to give its default channel model a pathLossType
 * default of "Tr38901" (see NrChannelModel_3GPP38_901.ned) instead of
 * NrChannelModel's own default of "Tr36873". The propagation formulas
 * themselves -- 3GPP TR 38.901, "Study on channel model for frequencies from
 * 0.5 to 100 GHz", v16.1.0, December 2019, including the building
 * penetration loss of its Section 7.4.3 -- live in Tr38901PathLossModel,
 * instantiated by the base class's createPathLossModel() when pathLossType
 * is "Tr38901". Every other behavior -- fading, interference, SINR
 * assembly, the reception decision -- comes from LteRealisticChannelModel.
 */
class NrChannelModel_3GPP38_901 : public NrChannelModel
{
};

} //namespace

#endif /* NRCHANNELMODEL_3GPP38_901_H_ */
