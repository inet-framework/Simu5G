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

#ifndef STACK_PHY_CHANNELMODEL_TR36814PATHLOSSMODEL_H_
#define STACK_PHY_CHANNELMODEL_TR36814PATHLOSSMODEL_H_

#include "simu5g/stack/phy/channelmodel/PathLossModel.h"

namespace simu5g {

/**
 * Path-loss, LOS-probability and shadowing-sigma formulas of
 * 3GPP TR 36.814, "Further advancements for E-UTRA physical layer aspects",
 * v9.2.0, March 2017, for the Indoor Hotspot (InH), Urban Microcell (UMi),
 * Urban Macrocell (UMa), Suburban Macrocell (SMa) and Rural Macrocell (RMa)
 * deployment scenarios.
 *
 * Every formula here works on the 3D coordinate distance; computePathLoss
 * and computeLosProbability ignore their d2D argument entirely.
 */
class Tr36814PathLossModel : public PathLossModel
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
    double computeAngularAttenuation(double hAngle, double vAngle) override;

  protected:
    /*
     * Select the shadowing sigma for the current scenario given whether the
     * link is below the scenario's breakpoint distance and its LOS state.
     * getShadowingStdDev() computes the breakpoint comparison itself and
     * calls this; Tr38901PathLossModel reuses it for the scenarios TR 38.901
     * does not cover, after computing its own (differently-defined) breakpoint.
     */
    double selectStdDev(const Deployment& deployment, bool belowBreakpoint, bool losState);

    /*
     * The check of the cell's building height and street width for the macro
     * scenarios of TR 36.814 and TR 36.873, which give the same range and the
     * same default values; 'study' names the table. Called by checkHeightRange().
     */
    virtual void checkLayoutRange(const Deployment& deployment, const char *study) const;

  private:
    double computeIndoor(double d, bool los);
    double computeUrbanMicro(const Deployment& deployment, double d, bool los);
    double computeUrbanMacro(const Deployment& deployment, double d, bool los);
    double computeSubUrbanMacro(const Deployment& deployment, double d, double& dbp, bool los);
    double computeRuralMacro(const Deployment& deployment, double d, double& dbp, bool los);
};

} //namespace

#endif
