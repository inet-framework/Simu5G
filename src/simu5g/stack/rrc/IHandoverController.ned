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
// Interface for the UE-side handover controller of the LTE/NR Stack.
//
moduleinterface IHandoverController
{
    parameters:
        @display("i=block/control");
}
