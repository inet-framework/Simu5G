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

package simu5g.stack.pdcp;

//
// Module interface for the RX-side leg merger of a multi-leg PDCP entity
// (see ~PdcpEntity): implementations merge the PDU streams of all legs into
// the single RX entity, whose reorder window restores SN order across legs.
// Instantiated by the PdcpEntity compound via a typename parameter when the
// bearer has more than one leg.
//
moduleinterface IPdcpLegJoiner
{
    parameters:
        @display("i=block/join");
    gates:
        input in[];      // per-leg input
        output out;      // merged stream to the RX entity
}
