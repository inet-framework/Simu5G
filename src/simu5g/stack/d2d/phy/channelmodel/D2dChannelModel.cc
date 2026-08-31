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

#include "simu5g/stack/d2d/phy/channelmodel/D2dChannelModel.h"

namespace simu5g {

Define_Module(D2dChannelModel);

void D2dChannelModel::initialize(int stage)
{
    StochasticChannelModel::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        rcvdSinrD2DSignal_ = cComponent::registerSignal("rcvdSinrD2D");
    }
}

std::vector<double> D2dChannelModel::getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, Coord destCoord)
{
    EV << "------------ GET RSRP D2D----------------" << endl;

    // D2D is like DL for the receivers, so the UE-side fading/shadowing maps apply.
    RadioLink link = medium_->d2dLink(this, lteInfo->getSourceId(), lteInfo->getCoord(), destId, destCoord);

    // Note the D2D-specific transmit power: a D2D transmission does not use the
    // power the UE would use towards the base station.
    return getRSRP(link, lteInfo->getD2dTxPower());
}

std::vector<double> D2dChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, Coord destCoord, MacNodeId enbId)
{
    // desired-signal RSRP (pathloss + shadowing + fading), then noise and
    // interference on top
    std::vector<double> rsrpVector = getRSRP_D2D(frame, lteInfo, destId, destCoord);
    return getSINR_D2D(frame, lteInfo, destId, destCoord, enbId, rsrpVector);
}

std::vector<double> D2dChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector)
{
    EV << "------------ GET SINR D2D----------------" << endl;

    // The desired signal is already known; the medium's computeInterferencePlusNoise
    // asks its D2D branch for the D2D denominator.
    RadioLink link = medium_->d2dLink(this, lteInfo->getSourceId(), lteInfo->getCoord(), destId, destCoord);
    link.cellId = enbId;

    // The caller is expected to supply one RSRP value per band. The one-to-many
    // capture-effect path only fills bestRsrpVector_ when the capture factor is
    // "RSRP"; with "Distance" it stays empty, and indexing it below would be out
    // of bounds. Fall back to computing the desired signal here rather than
    // reading past the end.
    if (rsrpVector.size() < numBands_) {
        if (!rsrpVector.empty())
            throw cRuntimeError("D2dChannelModel::getSINR_D2D - RSRP vector has %zu entries, expected %u",
                    rsrpVector.size(), numBands_);
        return getSINR(link, lteInfo, getRSRP(link, lteInfo->getD2dTxPower()));
    }

    return getSINR(link, lteInfo, rsrpVector);
}

} //namespace
