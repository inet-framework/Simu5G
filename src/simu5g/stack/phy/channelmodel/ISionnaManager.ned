//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel;

//
// Interface for the global owner of a ray-traced channel table (Plan A). It lives
// in the core (not under the optional Sionna feature) so a network can declare an
// optional `sionnaManager: <type> like ISionnaManager if hasSionnaManager` submodule
// without depending on the concrete ~SionnaManager being present. The concrete type
// (default "SionnaManager") is only resolved when the submodule is actually created.
//
moduleinterface ISionnaManager
{
    @display("i=block/server");
}
