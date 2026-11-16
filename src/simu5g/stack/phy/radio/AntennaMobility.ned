//
//                  Simu5G
//
// Copyright (C) 2026 Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.radio;

import inet.mobility.single.AttachedMobility;

//
// The mobility of an antenna: where its node's mobility is, at an offset, which
// is the antenna's height above the node (offsetZ). It is INET's
// ~AttachedMobility without a position of its own: that one, having no initial
// position parameters, draws a random position from its constraint area as it
// initializes, uses it for nothing, and fails where the area is unbounded.
// This one draws nothing, and has no constraint area: the node's mobility has
// one.
//
simple AntennaMobility extends AttachedMobility
{
    parameters:
        constraintAreaMinX = -inf m;
        constraintAreaMinY = -inf m;
        constraintAreaMinZ = -inf m;
        constraintAreaMaxX = inf m;
        constraintAreaMaxY = inf m;
        constraintAreaMaxZ = inf m;
        @class(AntennaMobility);
}
