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

#include "simu5g/stack/phy/channelmodel/PathLossModel.h"

namespace simu5g {

void PathLossModel::initialize(cComponent *owner,
        double carrierFrequencyHz, double carrierFrequencyGHz, double log10CarrierFrequencyGHz,
        bool tolerateMaxDistViolation)
{
    owner_ = owner;
    carrierFrequencyHz_ = carrierFrequencyHz;
    carrierFrequencyGHz_ = carrierFrequencyGHz;
    log10CarrierFrequencyGHz_ = log10CarrierFrequencyGHz;
    tolerateMaxDistViolation_ = tolerateMaxDistViolation;
}

void PathLossModel::throwNoPenetrationLoss(const Deployment& deployment, const char *study, bool definedInStudy) const
{
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    std::string scenario = DeploymentScenarioToA(deployment.scenario);
    if (definedInStudy)
        throw cRuntimeError("%s: the UE end of the link is inside a building (insideBuilding), and the building penetration loss of %s for the scenario %s is not implemented",
                owner.c_str(), study, scenario.c_str());
    throw cRuntimeError("%s: the UE end of the link is inside a building (insideBuilding), and %s defines no building penetration loss for the scenario %s",
            owner.c_str(), study, scenario.c_str());
}

} //namespace
