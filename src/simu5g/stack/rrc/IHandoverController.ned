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
// Interface for the handover controller of the ~IRrc layer: the UE-side
// component that tracks the serving cell of one stack leg and runs its
// handovers. One instance per leg; the ~Rrc compound instantiates the LTE and
// the NR one.
//
// An alternative implementation can change how a leg picks its serving cell --
// e.g. a leg that follows another leg's cell instead of scanning for the
// strongest one.
//
moduleinterface IHandoverController
{
    parameters:
        @display("i=block/control");
}
