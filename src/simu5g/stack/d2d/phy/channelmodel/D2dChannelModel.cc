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

#include "simu5g/stack/d2d/phy/channelmodel/D2dChannelModel.h"

#include "simu5g/stack/phy/channelmodel/NrChannelModel.h"
#include "simu5g/stack/phy/channelmodel/NrChannelModel_3GPP38_901.h"

namespace simu5g {

// D2D-capable channel models: the D2dChannelModel mixin layered over each core
// channel model. Only the concrete instantiations are Define_Module'd; all the
// D2D logic lives in the template (see D2dChannelModel.h).

class D2dRealisticChannelModel : public D2dChannelModel<LteRealisticChannelModel>
{
};

Define_Module(D2dRealisticChannelModel);

class D2dNrChannelModel : public D2dChannelModel<NrChannelModel>
{
};

Define_Module(D2dNrChannelModel);

class D2dNrChannelModel_3GPP38_901 : public D2dChannelModel<NrChannelModel_3GPP38_901>
{
};

Define_Module(D2dNrChannelModel_3GPP38_901);

} //namespace
