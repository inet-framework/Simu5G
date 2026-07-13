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

#include "simu5g/stack/sidelink/phy/SlIdealChannelModel.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(SlIdealChannelModel);

void SlIdealChannelModel::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

SlReceptionResult SlIdealChannelModel::computeReception(const SlAirFrameInfo& info, const inet::Coord& rxCoord, MacNodeId rxNodeId)
{
    SlReceptionResult result;
    result.rsrpDbm = -50;
    result.sinrDb = 60;
    result.sciDecoded = true;
    result.tbDecoded = true;
    return result;
}

} // namespace simu5g
