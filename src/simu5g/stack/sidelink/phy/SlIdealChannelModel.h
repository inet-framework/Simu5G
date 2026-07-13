//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIDELINK_SLIDEALCHANNELMODEL_H_
#define _SIDELINK_SLIDEALCHANNELMODEL_H_

#include "simu5g/stack/sidelink/phy/ISlChannelModel.h"

namespace simu5g {

/**
 * Ideal sidelink channel model: every frame (within the fan-out range) is
 * received; RSRP/SINR are fixed optimistic values. Default model of the SL
 * leg; useful for protocol testing and as the WP-C/M1 baseline.
 */
class SlIdealChannelModel : public omnetpp::cSimpleModule, public ISlChannelModel
{
  protected:
    void handleMessage(omnetpp::cMessage *msg) override;

  public:
    SlReceptionResult computeReception(const SlAirFrameInfo& info, const inet::Coord& rxCoord, MacNodeId rxNodeId) override;
};

} // namespace simu5g

#endif
