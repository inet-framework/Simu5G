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

#ifndef STACK_PHY_CHANNELMODEL_PATHLOSSMODEL_H_
#define STACK_PHY_CHANNELMODEL_PATHLOSSMODEL_H_

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

/**
 * Stateless propagation-formula strategy: one concrete subclass per 3GPP
 * propagation study (TR 36.814, TR 36.873, TR 38.901). A channel-model shell
 * owns an instance and calls it per link with the link's deployment (scenario
 * and heights) and the geometry (both distances, LOS state) it has already
 * resolved.
 *
 * All per-link state -- LOS map, shadowing history, fading maps, position
 * history -- stays in the owning channel model, and the facts of the link
 * being evaluated that its geometry does not give come with each call as
 * LinkConditions; an instance of this class holds nothing but the carrier
 * and a back-pointer to the owner for RNG draws, so that
 * every random draw a formula makes still comes from the same RNG stream as
 * before.
 */
class PathLossModel
{
  public:
    /**
     * The deployment a link is evaluated in: the scenario of its cell, and the
     * heights of its two ends and of the surroundings. It comes with each call.
     */
    struct Deployment
    {
        DeploymentScenario scenario = UNKNOWN_SCENARIO;
        double hNodeB = 0;     // the base station antenna's height [m]
        double hUe = 0;        // the UE antenna's height [m]
        double hBuilding = 0;  // the average building height [m]
        double wStreet = 0;    // the average street width [m]
        double clutterDensity = 0; // indoor factory: the fraction of the floor the clutter covers (r)
        double clutterHeight = 0;  // indoor factory: the effective clutter height (h_c) [m]
        double clutterSize = 0;    // indoor factory: the typical clutter size (d_clutter) [m]
    };

  protected:
    cComponent *owner_ = nullptr;  // RNG + parameter context

    double carrierFrequencyHz_ = 0;
    double carrierFrequencyGHz_ = 0;
    double log10CarrierFrequencyGHz_ = 0;
    bool tolerateMaxDistViolation_ = false;
    bool tolerateHeightViolation_ = false;

    /*
     * Propagation velocity in free space, as the breakpoint-distance notes of
     * all three reports define it. This is the rounded 3.0e8 m/s they write,
     * not the exact speed of light: the breakpoint distance appears in the
     * beyond-breakpoint path-loss formulas themselves and not only in the
     * branch condition, so the two choices differ by about 0.005 dB on every
     * LOS link past a breakpoint.
     */
    static constexpr double PROPAGATION_VELOCITY = 3.0e8;

    /*
     * The error of checkPenetrationLoss(): the study defines no building
     * penetration loss for the scenario, or defines one that the model lacks.
     */
    [[noreturn]] void throwNoPenetrationLoss(const Deployment& deployment, const char *study, bool definedInStudy) const;

    /*
     * The error of a formula asked for a scenario its study does not have.
     */
    [[noreturn]] void throwUnknownScenario(const Deployment& deployment) const;

    /*
     * The checks of checkHeights(). requireDefined() throws when 'ok' does
     * not hold: the scenario's formula for 'quantity' (the path loss, the LOS
     * probability) has no value at the link's heights.
     * requireInRange() throws when 'ok' does not hold, unless the violation
     * is tolerated by configuration; 'range' is the range as the study gives it.
     */
    void requireDefined(bool ok, const Deployment& deployment, const char *quantity, const char *condition) const;
    void requireInRange(bool ok, const Deployment& deployment, const char *study, const char *range) const;

    /*
     * The checks of the cell's building height h and street width W, for the
     * scenarios whose formulas take log10(h) and log10(W): requireLayoutDefined()
     * throws when either is not positive, whatever is tolerated;
     * requireLayoutInRange() throws when 'ok' does not hold, unless the violation
     * is tolerated by configuration (tolerateHeightViolation).
     */
    void requireLayoutDefined(const Deployment& deployment) const;
    void requireLayoutInRange(bool ok, const Deployment& deployment, const char *study, const char *range) const;

    /*
     * Throws if the link's heights are outside the range the study gives
     * for the scenario, and so are the cell's building height and street
     * width where the scenario's formulas take them; called by checkHeights().
     */
    virtual void checkHeightRange(const Deployment& deployment) const = 0;

  public:
    /**
     * The facts of the link being evaluated that its geometry does not give:
     * the link's effective environment height, and the UE end's building
     * penetration. The defaults are a link outdoors, at the h_E a link takes
     * without a draw.
     */
    struct LinkConditions
    {
        double environmentHeight = 1.0;  // h_E (TR 38.901 Table 7.4.1-1 NOTE 1, TR 36.873 7.2.1 NOTE 4)
        bool indoor = false;             // the UE end is inside a building
        double indoorDistance = 0.0;     // how far inside (d_2D-in)
        double penetrationDeviate = 0.0; // the UE's standard-normal penetration-loss deviate (TR 38.901 7.4.3.1)
    };

    virtual ~PathLossModel() {}

    /*
     * Copy the deployment-scenario parameters the formulas need. Parameter
     * ownership stays with the owning channel model; this only receives the
     * values it has already read from its NED parameters.
     */
    virtual void initialize(cComponent *owner,
            double carrierFrequencyHz, double carrierFrequencyGHz, double log10CarrierFrequencyGHz,
            bool tolerateMaxDistViolation, bool tolerateHeightViolation);

    /*
     * The base station antenna height the studies give for a scenario: 10 m
     * in an urban microcell, 25 m in an urban macrocell, 35 m in a suburban
     * or rural macrocell (TR 36.814 Table B.1.2.1-1, TR 36.873 Table 7.2-1,
     * TR 38.901 Table 7.4.1-1), and 3 m, the lowest of TR 36.814's 3-6 m,
     * in an indoor hotspot.
     */
    static double getDefaultBaseStationHeight(DeploymentScenario scenario);

    /*
     * Whether the scenario is one of the five indoor factory sub-scenarios
     * of TR 38.901 (InF-SL, InF-DL, InF-SH, InF-DH, InF-HH).
     */
    static bool isIndoorFactory(DeploymentScenario scenario)
    {
        return scenario == INDOOR_FACTORY_SL || scenario == INDOOR_FACTORY_DL || scenario == INDOOR_FACTORY_SH
            || scenario == INDOOR_FACTORY_DH || scenario == INDOOR_FACTORY_HH;
    }

    /*
     * Throws if the clutter of an indoor factory cell is not what TR 38.901
     * Table 7.2-4 gives for its sub-scenario: a density below 0.4 in a sparse
     * and of 0.4 or more in a dense one, an effective height of 0 to 10 m, a
     * positive size. Nothing is checked for another scenario. The radio of
     * the cell (owner) calls it with its parameters.
     */
    static void checkClutter(const cComponent *owner, DeploymentScenario scenario, double clutterDensity, double clutterHeight, double clutterSize);

    /*
     * Throws if the scenario's formulas cannot be evaluated at the link's
     * heights: where they have no value (an effective antenna height that is
     * not positive), and outside the range of heights the study gives for the
     * scenario; likewise the cell's building height and street width, where
     * the scenario's formulas take them. The owner calls it for every link it
     * evaluates.
     */
    void checkHeights(const Deployment& deployment) const;

    /*
     * Draw the effective environment height h_E of a link between a base
     * station site and a UE (TR 38.901 Table 7.4.1-1 NOTE 1, TR 36.873 7.2.1
     * NOTE 4), at the given 2D distance. 1 m, without a draw, where the study
     * and scenario have no other value or the probability of 1 m is 1.
     */
    virtual double drawEnvironmentHeight(const Deployment& deployment, double d2D) { return 1.0; }

    /*
     * Throws if the model has no building penetration loss for its scenario:
     * a link with its UE end inside a building cannot be evaluated then.
     * computePathLoss() calls it for such a link.
     */
    virtual void checkPenetrationLoss(const Deployment& deployment) const = 0;

    /*
     * Compute the path-loss attenuation according to the selected scenario,
     * for a link in the given conditions. A concrete model that needs only
     * one of the two distances ignores the other.
     */
    virtual double computePathLoss(const Deployment& deployment, double d3D, double d2D, bool los, const LinkConditions& conditions) = 0;

    /*
     * Compute the LOS probability according to the selected scenario.
     * Returns the probability p; the caller draws against it and stores the
     * outcome.
     */
    virtual double computeLosProbability(const Deployment& deployment, double d3D, double d2D) = 0;

    /*
     * Compute the standard deviation of the log-normal shadowing according
     * to the selected scenario and the LOS state of the link.
     */
    virtual double getShadowingStdDev(const Deployment& deployment, double d3D, double d2D, bool losState) = 0;

    /*
     * The standard deviation of the log-normal shadowing of a link whose UE end
     * is inside a building: the study's outdoor-to-indoor value, in LOS and NLOS
     * alike. An error where the study has no building penetration loss for the
     * scenario (as checkPenetrationLoss()).
     */
    virtual double getIndoorShadowingStdDev(const Deployment& deployment) = 0;

    /*
     * The correlation distance of a link's shadow fading in the horizontal
     * plane, in m, as the study's table of channel model parameters gives it
     * for the scenario and the link's propagation condition: LOS, NLOS, or
     * outdoor-to-indoor where the table has that column and the UE end of
     * the link is inside a building.
     */
    virtual double getShadowingCorrelationDistance(const Deployment& deployment, bool losState, const LinkConditions& conditions) = 0;

    /*
     * The correlation distance of a link's LOS/NLOS state, in m: how far an
     * end of the link moves before the state is decided again. Only TR 38.901
     * gives one (Table 7.6.3.1-2); the two older studies take its values,
     * which is a convention of the model. The suburban macrocell's value is
     * that of TR 38.901 v19.5.0, the first version whose table has it.
     */
    virtual double getLosCorrelationDistance(const Deployment& deployment);

    /*
     * Compute the attenuation caused by the antenna pattern for a given
     * reception direction.
     */
    virtual double computeAngularAttenuation(double hAngle, double vAngle) = 0;
};

} //namespace

#endif
