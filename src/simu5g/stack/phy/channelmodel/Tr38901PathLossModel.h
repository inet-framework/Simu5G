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

#ifndef STACK_PHY_CHANNELMODEL_TR38901PATHLOSSMODEL_H_
#define STACK_PHY_CHANNELMODEL_TR38901PATHLOSSMODEL_H_

#include "simu5g/stack/phy/channelmodel/Tr36873PathLossModel.h"

namespace simu5g {

/**
 * Path-loss, LOS-probability and shadowing-sigma formulas of 3GPP TR 38.901,
 * "Study on channel model for frequencies from 0.5 to 100 GHz", v16.1.0,
 * December 2019, for the Indoor Hotspot (InH), Urban Microcell (UMi-Street
 * Canyon), Urban Macrocell (UMa), Rural Macrocell (RMa) and Indoor Factory
 * (InF-SL, InF-DL, InF-SH, InF-DH, InF-HH) deployment scenarios, including
 * the building-penetration loss for indoor UEs (section 7.4.3) where the
 * model has it.
 *
 * An indoor factory link takes the clutter of its base station's hall from
 * the deployment (density r, effective height h_c, typical size d_clutter of
 * Table 7.2-4). The sub-scenario with both ends above the clutter, InF-HH,
 * is 100% LOS and has no NLOS formula: a link of it evaluated as NLOS is an
 * error.
 *
 * Scenario coverage is partial: Suburban Macrocell is not covered here and
 * falls through to the TR 36.873 formulas this class extends (which in turn
 * fall through to TR 36.814 for what neither covers). getShadowingStdDev()
 * computes this study's own breakpoint distance (Q3: 2*pi*hNodeB*hUe*fc/c,
 * unlike the base classes' 4*(hNodeB-1)*(hUe-1)*fc/c) and, for the scenarios
 * it does not cover, applies it against the shared TR 36.814 sigma table via
 * selectStdDev() rather than falling through to TR 36.873's
 * getShadowingStdDev() (which does not exist -- TR 36.873 has no shadowing
 * override of its own) or recomputing the base breakpoint.
 */
class Tr38901PathLossModel : public Tr36873PathLossModel
{
  public:
    void checkPenetrationLoss(const Deployment& deployment) const override;
    void checkHeightRange(const Deployment& deployment) const override;
    double computePathLoss(const Deployment& deployment, double d3D, double d2D, bool los, const LinkConditions& conditions) override;
    double computeLosProbability(const Deployment& deployment, double d3D, double d2D) override;
    double getShadowingStdDev(const Deployment& deployment, double d3D, double d2D, bool losState) override;
    double getIndoorShadowingStdDev(const Deployment& deployment) override;
    double computeIndoorDistance(const Deployment& deployment, const LinkConditions& conditions) override;
    double getShadowingCorrelationDistance(const Deployment& deployment, bool losState, const LinkConditions& conditions) override;
    double getLosCorrelationDistance(const Deployment& deployment) override;
    double drawEnvironmentHeight(const Deployment& deployment, double d2D) override;

    /*
     * Select the low-loss (default) or high-loss building-penetration model
     * of table 7.4.3-2 of TR 38.901 for the NLOS-through-wall term. Set by
     * the owning channel model from its "useBuildingPenetrationHighLossModel"
     * parameter.
     */
    void setUseBuildingPenetrationHighLossModel(bool value) { useBuildingPenetrationHighLossModel_ = value; }

  private:
    /*
     * Whether the single-frequency O2I model of Table 7.4.3-3 applies, rather
     * than Table 7.4.3-2: UMa and UMi below 6 GHz
     */
    virtual bool isSingleFrequencyO2iModel(const Deployment& deployment) const;

    double computeIndoor3D(double threeDimDistance, double twoDimDistance, bool los, const LinkConditions& conditions);
    double computeUrbanMicro3D(const Deployment& deployment, double threeDimDistance, double twoDimDistance, bool los, const LinkConditions& conditions);
    double computeUrbanMacro3D(const Deployment& deployment, double threeDimDistance, double twoDimDistance, bool los, const LinkConditions& conditions);
    double computeRuralMacro3D(const Deployment& deployment, double threeDimDistance, double twoDimDistance, bool los, const LinkConditions& conditions);
    double computeIndoorFactory(const Deployment& deployment, double threeDimDistance, bool los);

    /*
     * The error of an InF-HH link evaluated as NLOS: the sub-scenario has no
     * NLOS path loss and no NLOS shadowing.
     */
    [[noreturn]] void throwNoNlos(const Deployment& deployment) const;

    /*
     * Compute the building penetration loss for an indoor UE.
     * See section 7.4.3 of TR 38.901.
     */
    double computePenetrationLoss(const Deployment& deployment, double twoDimDistance, const LinkConditions& conditions);

    // flag for using the high-loss or low-loss building-penetration model
    // see table 7.4.3-2 in TR 38.901
    bool useBuildingPenetrationHighLossModel_ = false;
};

} //namespace

#endif
