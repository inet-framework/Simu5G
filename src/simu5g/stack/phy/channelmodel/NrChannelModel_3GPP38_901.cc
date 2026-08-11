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

#include "simu5g/stack/phy/channelmodel/NrChannelModel_3GPP38_901.h"

#include "simu5g/stack/phy/channelmodel/Tr38901PathLossModel.h"

namespace simu5g {

Define_Module(NrChannelModel_3GPP38_901);

void NrChannelModel_3GPP38_901::initialize(int stage)
{
    NrChannelModel::initialize(stage);
    if (inside_building_)
        useBuildingPenetrationHighLossModel_ = par("useBuildingPenetrationHighLossModel").boolValue();
    if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        check_and_cast<Tr38901PathLossModel *>(pathLoss_)->setUseBuildingPenetrationHighLossModel(useBuildingPenetrationHighLossModel_);
    }
}

PathLossModel *NrChannelModel_3GPP38_901::createPathLossModel()
{
    return new Tr38901PathLossModel();
}

void NrChannelModel_3GPP38_901::computeLosProbability(double d3D, double d2D, const LinkKey& nodeId)
{
    if (!dynamicLos_) {
        losMap_[nodeId] = fixedLos_;
        return;
    }
    double p = pathLoss_->computeLosProbability(d3D, d2D);
    losMap_[nodeId] = (uniform(0.0, 1.0) <= p);
}

double NrChannelModel_3GPP38_901::computePathLoss(double threeDimDistance, double twoDimDistance, bool los)
{
    return pathLoss_->computePathLoss(threeDimDistance, twoDimDistance, los);
}

} //namespace

