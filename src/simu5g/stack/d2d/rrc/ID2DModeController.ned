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

package simu5g.stack.d2d.rrc;

//
// Interface for the D2D mode controller of the ~IRrc layer: performs the
// D2D mode switch of a peering, draining and re-routing the affected RLC
// transmission buffers.
//
moduleinterface ID2DModeController
{
    parameters:
        @display("i=block/control");
}
