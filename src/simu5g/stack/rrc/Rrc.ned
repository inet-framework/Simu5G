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

package simu5g.stack.rrc;

//
// RRC (Radio Resource Control) module for LTE/NR networks.

// This is a blank implementation with no gates, serving as a placeholder
// for future RRC functionality development.
//
simple Rrc like IRrc
{
    parameters:
        @display("i=block/control");
        @class(Rrc);

        // Module references for accessing other stack layers
        string pdcpModule = default("^.pdcp");
        string rlcUmModule = default("^.rlc.um");
        string nrRlcUmModule = default("^.nrRlc.um");
        string macModule = default("^.mac");
        string nrMacModule = default("^.nrMac");
}
