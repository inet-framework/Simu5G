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

double NrChannelModel_3GPP38_901::computeShadowing(double d3D, double d2D, const LinkKey& nodeId, MacNodeId ownerId, double speed, bool cqiDl)
{
    ShadowFadingMap *actualShadowingMap;

    if (cqiDl) // If we are computing a DL CQI we need the Shadowing Map stored on the UE side
        actualShadowingMap = obtainShadowingMap(ownerId);
    else
        actualShadowingMap = &lastComputedSF_;

    if (actualShadowingMap == nullptr)
        throw cRuntimeError("NrChannelModel_3GPP38_901::computeShadowing - actualShadowingMap not found (nullptr)");

    double mean = 0;

    // Get std deviation according to los/nlos and selected scenario
    double stdDev = pathLoss_->getShadowingStdDev(d3D, d2D, losMap_[nodeId]);
    double time = 0;
    double space = 0;
    double att;

    // If direction is DOWNLINK it means that this module is located in UE stack than
    // the Move object associated with the UE is myMove_ variable
    // If direction is UPLINK it means that this module is located in UE stack than
    // the Move object associated with the UE is move variable

    // If shadowing for current user has never been computed
    if (actualShadowingMap->find(nodeId) == actualShadowingMap->end()) {
        // Get the log normal shadowing with std deviation stdDev
        att = normal(mean, stdDev);

        // Store the shadowing attenuation for this user and the temporal mark
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[nodeId] = tmp;

        // If the shadowing attenuation has been computed at least one time for this user
        // and the distance traveled by the UE is greater than correlation distance
    }
    else if ((NOW - actualShadowingMap->at(nodeId).first).dbl() * speed
             > correlationDistance_)
    {
        // Get the temporal mark of the last computed shadowing attenuation
        time = (NOW - actualShadowingMap->at(nodeId).first).dbl();

        // Compute the traveled distance
        space = time * speed;

        // Compute shadowing with an EAW (Exponential Average Window) (step1)
        double a = exp(-0.5 * (space / correlationDistance_));

        // Get last shadowing attenuation computed
        double old = actualShadowingMap->at(nodeId).second;

        // Compute shadowing with an EAW (Exponential Average Window) (step2)
        att = a * old + sqrt(1 - pow(a, 2)) * normal(mean, stdDev);

        // Store the new computed shadowing
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[nodeId] = tmp;

        // If the distance traveled by the UE is smaller than correlation distance shadowing attenuation remains the same
    }
    else {
        att = actualShadowingMap->at(nodeId).second;
    }
    EV << " NrChannelModel_3GPP38_901::computeShadowing - shadowing att = " << att << endl;

    return att;
}

} //namespace

