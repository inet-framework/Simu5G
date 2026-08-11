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

#ifndef NRCHANNELMODEL_3GPP38_901_H_
#define NRCHANNELMODEL_3GPP38_901_H_

#include "simu5g/stack/phy/channelmodel/NrChannelModel.h"

namespace simu5g {

/**
 * This channel model implements path loss, LOS probability, and shadowing according to
 * the following 3GPP specifications:
 * - 3GPP TR 38.901, "Study on channel model for frequencies from 0.5 to 100 GHz", v16.1.0, December 2019
 * - 3GPP TS 38.211, "NR; Physical channels and modulation", v16.2.0, July 2020
 * - 3GPP TS 38.214, "NR; Physical layer procedures for data", v16.2.0, July 2020
 *
 * The model supports 5G NR deployment scenarios including:
 * - Indoor Hotspot (InH) - 0.5-100 GHz
 * - Urban Microcell (UMi-Street Canyon) - 0.5-100 GHz
 * - Urban Macrocell (UMa) - 0.5-100 GHz
 * - Rural Macrocell (RMa) - 0.5-7 GHz
 *
 * Key features:
 * - Frequency-dependent path loss models compliant with 3GPP TR 38.901
 * - LOS/NLOS probability models for various scenarios
 * - Building penetration loss for indoor UEs (Section 7.4.3 of TR 38.901)
 * - Proper frequency handling: Hz for physical calculations, GHz for path loss formulas
 */
class NrChannelModel_3GPP38_901 : public NrChannelModel
{

  public:
    void initialize(int stage) override;

    /*
     * Compute LOS probability (taken from TR 38.901)
     *
     * @param d3D 3D distance between UE and gNodeB
     * @param d2D 2D distance between UE and gNodeB
     * @param nodeId mac node id of UE
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
     * Compute shadowing
     *
     * @param d3D 3D distance between UE and gNodeB
     * @param d2D 2D distance between UE and gNodeB
     * @param nodeId mac node id of UE
     * @param speed speed of UE
     */
    double computeShadowing(double d3D, double d2D, const LinkKey& key, MacNodeId ownerId, double speed, bool cqiDl) override;

  protected:

    /*
     * Create the strategy object supplying the propagation formulas: the
     * TR 38.901 ones.
     */
    PathLossModel *createPathLossModel() override;
};

} //namespace

#endif /* NRCHANNELMODEL_3GPP38_901_H_ */
