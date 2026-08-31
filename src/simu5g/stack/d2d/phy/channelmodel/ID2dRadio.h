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

#ifndef _LTE_ID2DRADIO_H_
#define _LTE_ID2DRADIO_H_

#include <vector>

#include <inet/common/geometry/common/Coord.h>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

class LteAirFrame;
class UserControlInfo;

/*
 * Interface implemented by the D2D-capable radio (D2dRadio).
 *
 * D2D PHY code talks to the radio's D2D reception/feedback machinery through
 * this interface instead of the concrete radio class, so that the core
 * radio (Radio) carries no D2D code. Obtain it with
 * check_and_cast<ID2dRadio *>(channelModel): a D2D NIC always wires a D2D
 * radio into its radio/nrRadio slots.
 */
class ID2dRadio
{
  public:
    virtual ~ID2dRadio() {}

    /// Compute the received useful signal (RSRP) for a D2D transmission, per band.
    virtual std::vector<double> getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, inet::Coord destCoord) = 0;

    /// Compute the D2D SINR towards the given peer, per band (path loss, shadowing, fading and D2D interference).
    virtual std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId peerUeId, inet::Coord peerUeCoord, MacNodeId enbId = NODEID_NONE) = 0;

    /// Compute the D2D SINR from a precomputed RSRP vector (used for one-to-many D2D reception).
    virtual std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector) = 0;
};

} //namespace

#endif
