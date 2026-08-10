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
 * Replaces the propagation formulas of LteRealisticChannelModel with the 3D ones
 * of TR 36.873, and inherits the rest of the PHY link model from it unchanged --
 * fading, interference, SINR assembly and the reception decision all still come
 * from the base class.
 *
 * - 3GPP TR 36.873, "Study on 3D channel model for LTE", v12.7.0, December 2017
 *
 * "3D" is what these formulas add: path loss is evaluated over the 3D distance
 * between the endpoints while the LOS probability and the validity limits remain
 * functions of the 2D distance, so the base-station and UE heights (nodebHeight,
 * ueHeight) now enter the result. The antenna pattern gains a vertical component
 * alongside the horizontal one.
 *
 * Scenario coverage is partial, and what is missing falls through to the base
 * class rather than being an error:
 * - path loss: Indoor Hotspot, Urban Microcell, Urban Macrocell and Rural
 *   Macrocell are computed here; Suburban Macrocell falls through.
 * - LOS probability: Urban Microcell, Urban Macrocell and Rural Macrocell are
 *   computed here; Indoor Hotspot and Suburban Macrocell fall through, so an
 *   indoor deployment draws LOS from the base class's TR 36.814 formula and its
 *   path loss from TR 36.873.
 *
 * The name records the deployment this model is the default for (the gNodeB and
 * NR UE NICs), not a property of TR 36.873, which is itself an LTE study item.
 */
class NrChannelModel : public LteRealisticChannelModel
{

  public:
    void initialize(int stage) override;

    /*
     * Compute attenuation caused by path loss and shadowing (optional)
     *
     * @param nodeId MAC node ID of UE
     * @param dir traffic direction
     * @param coord position of end point communication (if dir==UL it is the position of UE else it is the position of gNodeB)
     */
    double getAttenuation(const RadioLink& link) override;
    using LteRealisticChannelModel::getAttenuation; // keep the cellular convenience overload visible

    /*
     *  Compute attenuation caused by transmission direction
     *
     * @param angle angle
     */
    double computeAngularAttenuation(double hAngle, double vAngle) override;

    /*
     * Compute LOS probability (taken from TR 36.873)
     *
     * @param d distance between UE and gNodeB
     * @param nodeId MAC node ID of UE
     */
    void computeLosProbability(double d, const LinkKey& key) override;

    /*
     * Compute the path-loss attenuation according to the selected scenario
     *
     * @param threeDimDistance distance between UE and gNodeB (3D)
     * @param twoDimDistance distance between UE and gNodeB (2D)
     * @param los line-of-sight flag
     */
    double computePathLoss(double threeDimDistance, double twoDimDistance, bool los) override;

    /*
     * 3D-InH path loss model (taken from TR 36.873)
     *
     * @param threeDimDistance distance between UE and gNodeB
     * @param los line-of-sight flag
     */
    virtual double computeIndoor3D(double threeDimDistance, double twoDimDistance, bool los);

    /*
     * 3D-UMi path loss model (taken from TR 36.873)
     *
     * @param threeDimDistance distance between UE and gNodeB
     * @param los line-of-sight flag
     */
    virtual double computeUrbanMicro3D(double threeDimDistance, double twoDimDistance, bool los);

    /*
     * 3D-UMa path loss model (taken from TR 36.873)
     *
     * @param threeDimDistance distance between UE and gNodeB
     * @param los line-of-sight flag
     */
    virtual double computeUrbanMacro3D(double threeDimDistance, double twoDimDistance, bool los);

    /*
     * 3D-RMa path loss model (taken from TR 36.873)
     *
     * @param threeDimDistance distance between UE and gNodeB
     * @param los line-of-sight flag
     */
    virtual double computeRuralMacro3D(double threeDimDistance, double twoDimDistance, bool los);

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
};

} //namespace

#endif /* NRCHANNELMODEL_H_ */

