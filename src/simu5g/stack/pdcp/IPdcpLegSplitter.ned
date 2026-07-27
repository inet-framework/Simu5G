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
// Module interface for the TX-side leg dispatcher of a multi-leg PDCP entity
// (see ~PdcpEntity): implementations pick the RLC leg for each PDCP PDU and
// apply that leg's id mapping. Instantiated by the PdcpEntity compound via a
// typename parameter when the bearer has more than one leg.
//
moduleinterface IPdcpLegSplitter
{
    parameters:
        @display("i=block/dispatch");
    gates:
        input in;        // PDUs from the TX entity
        output out[];    // to leg k
}
