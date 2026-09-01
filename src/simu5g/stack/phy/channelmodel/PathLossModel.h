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
 * The transmitting/receiving radio's outdoor-to-indoor geometry: TR 38.901
 * clause 7.4.3, TR 36.873 clause 7.2.3. Per-radio, so it travels as a call
 * argument to computePathLoss() rather than as cached strategy state.
 */
struct O2iState
{
    bool insideBuilding = false;
    double insideDistance = 0;
};

/**
 * The per-*link* facts a propagation formula needs beyond distance/LOS/O2I:
 * the carrier frequency triple and the two antenna heights. Threaded per
 * call rather than cached at initialize() time -- the carrier frequency
 * because a single pathLoss submodule per leg entry could eventually serve
 * more than one frequency; the heights because they are per-*node* facts
 * and a link's two ends need not be the same pair of nodes twice in a row
 * (background UEs, D2D peers, ...).
 *
 * hNodeB/hUe name the two roles the formulas were written against (base
 * station / UE), resolved per link as h_BS from the link's eNB-role
 * endpoint and h_UT from its UE-role endpoint, regardless of which end is
 * transmitting -- the role is intrinsic to the node id
 * (getNodeTypeById()), not to the link's tx/rx labelling.
 */
struct LinkContext
{
    double carrierFrequencyHz = 0;
    double carrierFrequencyGHz = 0;
    double log10CarrierFrequencyGHz = 0;
    double hNodeB = 0;
    double hUe = 0;
};

/**
 * Stateless propagation-formula strategy: one concrete subclass per 3GPP
 * propagation study (TR 36.814, TR 36.873, TR 38.901). The medium owns one
 * instance per carrier leg, feeds it the deployment-scenario parameters once
 * at initialize() time, and calls it per link with the geometry (both
 * distances, LOS state) it has already resolved.
 *
 * All per-link state -- LOS map, shadowing history, fading maps, position
 * history -- stays in the medium; an instance of this class
 * holds nothing but the cached scenario parameters and a back-pointer to the
 * owner for RNG draws, so that every random draw a formula makes comes from
 * the owner's own rng-0 stream.
 */
class PathLossModel
{
  protected:
    cComponent *owner_ = nullptr;  // RNG context only: no par() reads

    DeploymentScenario scenario_ = UNKNOWN_SCENARIO;
    double hBuilding_ = 0;
    double wStreet_ = 0;
    bool tolerateMaxDistViolation_ = false;

    /*
     * Propagation velocity in free space, as the breakpoint-distance notes of
     * all three reports define it. This is the rounded 3.0e8 m/s they write,
     * not the exact speed of light: the breakpoint distance appears in the
     * beyond-breakpoint path-loss formulas themselves and not only in the
     * branch condition, so the two choices differ by about 0.005 dB on every
     * LOS link past a breakpoint.
     */
    static constexpr double PROPAGATION_VELOCITY = 3.0e8;

  public:
    virtual ~PathLossModel() {}

    /*
     * Copy the deployment-scenario parameters the formulas need. Parameter
     * ownership stays with the carrier leg module; this only receives the
     * values it has already read from its NED parameters. The carrier
     * frequency and the two antenna heights are not among them: they
     * travel per call, in a LinkContext.
     */
    virtual void initialize(cComponent *owner, DeploymentScenario scenario,
            double hBuilding, double wStreet, bool tolerateMaxDistViolation);

    /*
     * Compute the path-loss attenuation according to the selected scenario.
     * A concrete model that needs only one of the two distances ignores the
     * other. o2i is the calling radio's own outdoor-to-indoor geometry, read
     * by the scenarios that model building-penetration loss. link carries
     * this call's carrier frequency and antenna heights.
     */
    virtual double computePathLoss(double d3D, double d2D, bool los, const O2iState& o2i, const LinkContext& link) = 0;

    /*
     * Compute the LOS probability according to the selected scenario.
     * Returns the probability p; the caller draws against it and stores the
     * outcome. link carries this call's carrier frequency and antenna
     * heights; most scenarios need only link.hUe, none needs the
     * frequency, but the one bundle is passed uniformly (as O2iState already
     * is to computePathLoss, unused by the scenarios that ignore it).
     */
    virtual double computeLosProbability(double d3D, double d2D, const LinkContext& link) = 0;

    /*
     * Compute the standard deviation of the log-normal shadowing according
     * to the selected scenario and the LOS state of the link. link carries
     * this call's carrier frequency and antenna heights.
     */
    virtual double getShadowingStdDev(double d3D, double d2D, bool losState, const LinkContext& link) = 0;

    /*
     * Compute the attenuation caused by the antenna pattern for a given
     * reception direction.
     */
    virtual double computeAngularAttenuation(double hAngle, double vAngle) = 0;
};

} //namespace

#endif
