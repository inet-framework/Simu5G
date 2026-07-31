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
// Interface for the bearer management component of the ~IRrc layer: creates and
// tears down the PDCP, RLC and MAC entities of data radio bearers.
//
// An alternative implementation can change how a bearer is laid out over the
// node's stack legs -- e.g. how many legs a bearer has, and which leg each of
// its PDCP/RLC entities is installed on. See ~BearerManagement for the model
// that implements the LTE / NR dual connectivity layout.
//
moduleinterface IBearerManagement
{
    parameters:
        @display("i=block/control");
}
