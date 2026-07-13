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

package simu5g.stack.d2d.binder;

//
// Global holder of the D2D-specific network state, split out of the core ~Binder.
//
// It keeps the D2D peering map (whether pairs of D2D-capable UEs communicate in
// Direct or Infrastructure Mode) and the set of D2D one-to-many transmitters,
// and answers the D2D capability/mode queries derived from them.
//
// A single instance exists per network. It is not placed in network NED files:
// the first D2D module that needs it creates it dynamically under the network
// module (see D2dBinder::getInstance()). The module is a passive state
// container with no events, signals or statistics.
//
simple D2dBinder
{
    parameters:
        string binderModule = default("binder");
        @display("i=block/cogwheel");
}
