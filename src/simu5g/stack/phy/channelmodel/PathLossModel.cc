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

void PathLossModel::requireDefined(bool ok, const Deployment& deployment, const char *quantity, const char *condition) const
{
    if (ok)
        return;
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: %s above the ground (z = 0), and the %s of the scenario %s is defined only where %s; "
            "an antenna's height is its node's z plus its antenna.mobility.offsetZ",
            owner.c_str(), describeHeights(deployment).c_str(), quantity, DeploymentScenarioToA(deployment.scenario).c_str(), condition);
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

static std::string describeLayout(const PathLossModel::Deployment& deployment)
{
    std::stringstream os;
    os << "the cell's building height is " << deployment.hBuilding << " m and its street width " << deployment.wStreet
       << " m (the buildingHeight and streetWidth of its base station node)";
    return os.str();
}

void PathLossModel::requireLayoutDefined(const Deployment& deployment) const
{
    if (deployment.hBuilding > 0 && deployment.wStreet > 0)
        return;
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: %s, and the path loss of the scenario %s takes their logarithms: both must be above 0 m",
            owner.c_str(), describeLayout(deployment).c_str(), DeploymentScenarioToA(deployment.scenario).c_str());
}

void PathLossModel::requireLayoutInRange(bool ok, const Deployment& deployment, const char *study, const char *range) const
{
    if (ok || tolerateHeightViolation_)
        return;
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: %s, outside the range %s gives for the scenario %s (%s); tolerateHeightViolation switches this check off",
            owner.c_str(), describeLayout(deployment).c_str(), study, DeploymentScenarioToA(deployment.scenario).c_str(), range);
}

double PathLossModel::getDefaultBaseStationHeight(DeploymentScenario scenario)
{
    switch (scenario) {
        case INDOOR_HOTSPOT: return 3;
        case URBAN_MICROCELL: return 10;
        case URBAN_MACROCELL: return 25;
        case SUBURBAN_MACROCELL:
        case RURAL_MACROCELL: return 35;
        case INDOOR_FACTORY_SL:
        case INDOOR_FACTORY_DL:
        case INDOOR_FACTORY_SH:
        case INDOOR_FACTORY_DH:
        case INDOOR_FACTORY_HH:
            throw cRuntimeError("A link between two UEs cannot be evaluated in the scenario %s: no propagation study has such a link, "
                    "and the model's convention for it, the base station height the studies give for the scenario, has no value in an indoor factory",
                    DeploymentScenarioToA(scenario).c_str());
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

void PathLossModel::checkClutter(const cComponent *owner, DeploymentScenario scenario, double clutterDensity, double clutterHeight, double clutterSize)
{
    if (!isIndoorFactory(scenario))
        return;
    std::string name = DeploymentScenarioToA(scenario);
    const char *table = "TR 38.901 Table 7.2-4";
    // the sub-scenario with both ends above the clutter takes any clutter density
    if (scenario != INDOOR_FACTORY_HH) {
        if (!(clutterDensity > 0 && clutterDensity < 1))
            throw cRuntimeError(owner, "clutterDensity = %g: the clutter density of the scenario %s is the fraction of the floor the clutter covers, above 0 and below 1",
                    clutterDensity, name.c_str());
        bool dense = scenario == INDOOR_FACTORY_DL || scenario == INDOOR_FACTORY_DH;
        if (dense && clutterDensity < 0.4)
            throw cRuntimeError(owner, "clutterDensity = %g: the scenario %s has dense clutter, a density of 0.4 or more (%s); a hall with less is a sparse sub-scenario (INDOOR_FACTORY_SL, INDOOR_FACTORY_SH)",
                    clutterDensity, name.c_str(), table);
        if (!dense && clutterDensity >= 0.4)
            throw cRuntimeError(owner, "clutterDensity = %g: the scenario %s has sparse clutter, a density below 0.4 (%s); a hall with more is a dense sub-scenario (INDOOR_FACTORY_DL, INDOOR_FACTORY_DH)",
                    clutterDensity, name.c_str(), table);
    }
    if (!(clutterHeight >= 0 && clutterHeight <= 10))
        throw cRuntimeError(owner, "clutterHeight = %g m: the effective clutter height of the scenario %s is 0 to 10 m (%s)",
                clutterHeight, name.c_str(), table);
    if (!(clutterSize > 0))
        throw cRuntimeError(owner, "clutterSize = %g m: the typical clutter size of the scenario %s must be positive",
                clutterSize, name.c_str());
}

void PathLossModel::throwUnknownScenario(const Deployment& deployment) const
{
    std::string owner = owner_ != nullptr ? owner_->getFullPath() : "path loss model";
    throw cRuntimeError("%s: the propagation study the channel model takes its formulas from (pathLossType) has no scenario %s%s",
            owner.c_str(), DeploymentScenarioToA(deployment.scenario).c_str(),
            isIndoorFactory(deployment.scenario) ? "; the indoor factory scenarios are TR 38.901's: set pathLossType = \"Tr38901\" on the channel model" : "");
}

void PathLossModel::checkHeights(const Deployment& deployment) const
{
    switch (deployment.scenario) {
        case URBAN_MICROCELL:
        case URBAN_MACROCELL:
            // the breakpoint distance takes the heights above an environment of 1 m or more
            requireDefined(deployment.hNodeB > 1.0 && deployment.hUe > 1.0, deployment, "path loss", "both antennas are higher than 1 m");
            break;
        case RURAL_MACROCELL:
        case SUBURBAN_MACROCELL:
            requireDefined(deployment.hNodeB > 0.0 && deployment.hUe > 0.0, deployment, "path loss", "both antennas are above the ground");
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
