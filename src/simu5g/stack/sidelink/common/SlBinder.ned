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

package simu5g.stack.sidelink.common;

//
// Global sidelink registry: L2-ID mapping and allocation, group membership,
// SL carrier registry and the SL transmission map. The sidelink analog of
// ~Binder. A single instance exists per network; it is found or dynamically
// created under the network module on first use, so network NED files do
// not need to declare it.
//
simple SlBinder
{
    parameters:
        @display("i=block/cogwheel");
}
