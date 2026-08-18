//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.corenetwork.smf;

//
// Session Management Function (SMF) of the core network.
//
// The session management of the core network is what decides which data radio bearers
// exist, what they look like, and when they are set up: it configures the RAN, and the
// RAN never authors a bearer of its own. This module is where that role lives.
//
// Only one instance of this module is permitted within the entire network.
//
simple Smf
{
    parameters:
        @display("i=block/cogwheel");
}
