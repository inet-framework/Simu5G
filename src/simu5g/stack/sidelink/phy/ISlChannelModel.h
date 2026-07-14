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

#ifndef _SIDELINK_ISLCHANNELMODEL_H_
#define _SIDELINK_ISLCHANNELMODEL_H_

#include <inet/common/geometry/common/Coord.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

class SlAirFrameInfo;

/**
 * Result of receiving one sidelink frame at one receiver (design decision D7).
 * The PSSCH decode decision itself is made by the PHY: it combines
 * tbErrorProb with the receiver-side HARQ attempt count (blind
 * retransmission soft combining, WP-F) before drawing the outcome.
 */
struct SlReceptionResult
{
    double rsrpDbm = 0;      // SL-RSRP: received signal power over the frame's subchannels
                             // (pathloss+shadowing only, no noise/interference)
    double sinrDb = 0;       // SINR over the frame's subchannels (interference from the
                             // SL transmission map + thermal noise)
    bool sciDecoded = false; // PSCCH decode (threshold rule, D11)
    double tbErrorProb = 0;  // PSSCH TB error probability before HARQ combining
                             // (per-CQI BLER over the used PRBs); meaningless unless sciDecoded
};

/**
 * C++ interface of sidelink channel models (design decision D7), consumed by
 * NrSlPhyUe via dynamic_cast. Implementations are NED modules instantiated in
 * the NIC's slChannelModel slot.
 */
class ISlChannelModel
{
  public:
    virtual ~ISlChannelModel() {}

    /// compute the reception of a frame (described by its modeled SCI content
    /// and TX metadata) at position rxCoord of node rxNodeId
    virtual SlReceptionResult computeReception(const SlAirFrameInfo& info, const inet::Coord& rxCoord, MacNodeId rxNodeId) = 0;

    /// per-extra-attempt error-probability scaling factor of the blind-HARQ
    /// soft-combining model (the Uu error model's harqReduction convention)
    virtual double getHarqReduction() const { return 0.2; }
};

} // namespace simu5g

#endif
