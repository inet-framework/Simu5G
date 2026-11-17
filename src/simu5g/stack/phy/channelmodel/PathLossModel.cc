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

#include <sstream>

#include "simu5g/stack/phy/channelmodel/PathLossModel.h"

namespace simu5g {

void PathLossModel::initialize(cComponent *owner,
        double carrierFrequencyHz, double carrierFrequencyGHz, double log10CarrierFrequencyGHz,
        bool tolerateMaxDistViolation, bool tolerateHeightViolation)
{
    owner_ = owner;
    carrierFrequencyHz_ = carrierFrequencyHz;
    carrierFrequencyGHz_ = carrierFrequencyGHz;
    log10CarrierFrequencyGHz_ = log10CarrierFrequencyGHz;
    tolerateMaxDistViolation_ = tolerateMaxDistViolation;
    tolerateHeightViolation_ = tolerateHeightViolation;
}

static std::string describeHeights(const PathLossModel::Deployment& deployment)
{
    std::stringstream os;
    os << "the link's base station antenna is at " << deployment.hNodeB << " m and its UE antenna at " << deployment.hUe << " m";
    return os.str();
}

void PathLossModel::requireDefined(bool ok, const Deployment& deployment, const char *condition) const
{
    if (ok)
        return;
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: %s above the ground (z = 0), and the path loss of the scenario %s is defined only where %s; "
            "an antenna's height is its node's z plus its antenna.mobility.offsetZ",
            owner.c_str(), describeHeights(deployment).c_str(), DeploymentScenarioToA(deployment.scenario).c_str(), condition);
}

void PathLossModel::requireInRange(bool ok, const Deployment& deployment, const char *study, const char *range) const
{
    if (ok || tolerateHeightViolation_)
        return;
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: %s above the ground (z = 0), outside the range %s gives for the scenario %s (%s); "
            "an antenna's height is its node's z plus its antenna.mobility.offsetZ, and tolerateHeightViolation switches this check off",
            owner.c_str(), describeHeights(deployment).c_str(), study, DeploymentScenarioToA(deployment.scenario).c_str(), range);
}

double PathLossModel::getDefaultBaseStationHeight(DeploymentScenario scenario)
{
    switch (scenario) {
        case INDOOR_HOTSPOT: return 3;
        case URBAN_MICROCELL: return 10;
        case URBAN_MACROCELL: return 25;
        case SUBURBAN_MACROCELL:
        case RURAL_MACROCELL: return 35;
        default: throw cRuntimeError("PathLossModel: no base station height for scenario %d", (int)scenario);
    }
}

double PathLossModel::getLosCorrelationDistance(const Deployment& deployment)
{
    // TR 38.901 Table 7.6.3.1-2, "LOS/NLOS state"
    switch (deployment.scenario) {
        case RURAL_MACROCELL: return 60;
        case URBAN_MICROCELL:
        case URBAN_MACROCELL:
        case SUBURBAN_MACROCELL: return 50;
        case INDOOR_HOTSPOT: return 10;
        default: throw cRuntimeError("PathLossModel: no LOS correlation distance for the scenario %s", DeploymentScenarioToA(deployment.scenario).c_str());
    }
}

void PathLossModel::checkHeights(const Deployment& deployment) const
{
    switch (deployment.scenario) {
        case URBAN_MICROCELL:
        case URBAN_MACROCELL:
            // the breakpoint distance takes the heights above an environment of 1 m or more
            requireDefined(deployment.hNodeB > 1.0 && deployment.hUe > 1.0, deployment, "both antennas are higher than 1 m");
            break;
        case RURAL_MACROCELL:
        case SUBURBAN_MACROCELL:
            requireDefined(deployment.hNodeB > 0.0 && deployment.hUe > 0.0, deployment, "both antennas are above the ground");
            break;
        default:
            break;
    }
    checkHeightRange(deployment);
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
