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

#include "simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h"

namespace simu5g {

Define_Module(SionnaChannelModel);

void SionnaChannelModel::initialize(int stage)
{
    NrChannelModel::initialize(stage);
    // TODO(01-03): at INITSTAGE_LOCAL, resolve the SionnaManager module and acquire
    // the loaded SionnaTable handle used by getAttenuation().
}

double SionnaChannelModel::getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl)
{
    // TODO(01-03): replace with -table_->lookup(linkKeyFor(nodeId, dir, coord)).
    // The real body keeps the OMNeT++ distance computation
    // (phy_->getCoord().distance(coord)) to prove the TOOL-02 coord transform, then
    // returns the negated Sionna path gain instead of the analytic computePathLoss.
    return NrChannelModel::getAttenuation(nodeId, dir, coord, cqiDl);
}

} //namespace
