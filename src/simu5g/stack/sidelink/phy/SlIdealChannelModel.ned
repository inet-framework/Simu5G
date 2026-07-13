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

package simu5g.stack.sidelink.phy;

import simu5g.stack.phy.channelmodel.ILteChannelModel;

//
// Ideal sidelink channel model: every frame within the fan-out range is
// received (no pathloss, interference or errors). Default of the SL leg.
//
simple SlIdealChannelModel like ILteChannelModel
{
    parameters:
        @display("i=block/classifier2");
}
