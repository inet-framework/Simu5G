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

#ifndef NRCHANNELMODEL_H_
#define NRCHANNELMODEL_H_

#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"

namespace simu5g {

/**
 * Replaces the propagation formulas of LteRealisticChannelModel with the ones
 * of TR 36.873, and inherits the rest of the PHY link model from it unchanged --
 * fading, interference, SINR assembly and the reception decision all still come
 * from the base class.
 *
 * - 3GPP TR 36.873, "Study on 3D channel model for LTE", v12.7.0, December 2017
 *
 * The formulas themselves live in a Tr36873PathLossModel strategy, selected by
 * overriding createPathLossModel(); computePathLoss, computeLosProbability and
 * computeAngularAttenuation are thin delegations to it. getAttenuation is
 * inherited unchanged from LteRealisticChannelModel, which computes both the
 * 3D and 2D distance between the endpoints and passes both down: path loss is
 * evaluated over the 3D distance while the LOS probability and the validity
 * limits remain functions of the 2D distance, so the base-station and UE
 * heights (nodebHeight, ueHeight) enter the result through both, and the
 * antenna pattern gains a vertical component alongside the horizontal one.
 * The external-cell interference methods also still carry their own bodies.
 *
 * The name records the deployment this model is the default for (the gNodeB and
 * NR UE NICs), not a property of TR 36.873, which is itself an LTE study item.
 */
class NrChannelModel : public LteRealisticChannelModel
{

  public:
    void initialize(int stage) override;

    /*
     * Compute LOS probability (taken from TR 36.873)
     *
     * @param d3D 3D distance between UE and gNodeB
     * @param d2D 2D distance between UE and gNodeB
     * @param nodeId MAC node ID of UE
     */
    void computeLosProbability(double d3D, double d2D, const LinkKey& key) override;

    /*
     * Compute the path-loss attenuation according to the selected scenario
     *
     * @param threeDimDistance distance between UE and gNodeB (3D)
     * @param twoDimDistance distance between UE and gNodeB (2D)
     * @param los line-of-sight flag
     */
    double computePathLoss(double threeDimDistance, double twoDimDistance, bool los) override;

    /*
     * Evaluates total interference from external cells seen from the spot given by coord
     * @return total interference expressed in dBm
     */
    bool computeExtCellInterference(MacNodeId eNbId, MacNodeId nodeId, inet::Coord coord, bool isCqi, GHz carrierFrequency, std::vector<double> *interference) override;

    /*
     * Compute attenuation due to path loss and shadowing
     * @return attenuation expressed in dBm
     */
    virtual double computeExtCellPathLoss3D(double threeDimDistance, double twoDimDistance, MacNodeId nodeId);

  protected:

    /*
     * Create the strategy object supplying the propagation formulas: the
     * TR 36.873 ones.
     */
    PathLossModel *createPathLossModel() override;
};

} //namespace

#endif /* NRCHANNELMODEL_H_ */

