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
// Module interface for the receiving side of a PDCP entity. Concrete types
// (LteRxPdcpEntity, NrRxPdcpEntity) implement it so the PdcpEntity/NrPdcpEntity
// compound can instantiate them via a typename parameter. The dual-connectivity
// gate (dcIn) is @loose: it is wired only on nodes that have a DcMux (X2).
//
moduleinterface IRxPdcpEntity
{
    parameters:
        @display("i=block/layer");
    gates:
        input in;            // PDUs from the RLC entity
        input dcIn @loose;   // from DcMux (NR dual connectivity, X2)
        output out;          // reassembled SDUs to the upper mux
}
