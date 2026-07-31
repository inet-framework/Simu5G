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
// Interface for the registration component of the ~IRrc layer: registers the
// node (and its stack legs) with the ~Binder, sets up the network interface,
// and joins multicast groups.
//
moduleinterface IRegistration
{
    parameters:
        @display("i=block/control");
}
