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

#ifndef SIONNACHANNELMODEL_H_
#define SIONNACHANNELMODEL_H_

#include "simu5g/stack/phy/channelmodel/NrChannelModel.h"

namespace simu5g {

//
// Opt-in channel model that replaces the analytic 3GPP path loss with a path gain
// derived from an offline Sionna RT precompute. It subclasses NrChannelModel so it
// satisfies the ILteChannelModel interface (via the parent chain) with no NED-interface
// edit (SEAM-01). When this model is active it fully owns path gain; the inherited
// statistical shadowing/LOS/penetration terms are left inert by configuration.
//
// This is a compilable skeleton; the real table-lookup body is filled in by Plan 01-03.
//
class SionnaChannelModel : public NrChannelModel
{
  public:
    void initialize(int stage) override;

    /*
     * Compute attenuation (dB) for a link. In the final implementation this returns
     * -pathGain_dB from the Sionna table rather than the analytic path loss.
     *
     * @param nodeId MAC node ID of UE
     * @param dir traffic direction
     * @param coord position of end point communication (if dir==UL it is the position of UE else it is the position of gNodeB)
     */
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl) override;
};

} //namespace

#endif /* SIONNACHANNELMODEL_H_ */
