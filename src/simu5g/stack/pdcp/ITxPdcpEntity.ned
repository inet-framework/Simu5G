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
// Module interface for the transmitting side of a PDCP entity. Concrete types
// (LteTxPdcpEntity, NrTxPdcpEntity) implement it so the PdcpEntity compound
// can instantiate them via a typename parameter. The TX entity is leg-agnostic
// (SN assignment, header compression); leg dispatch is the enclosing
// compound's concern (see ~PdcpEntity).
//
moduleinterface ITxPdcpEntity
{
    parameters:
        @display("i=block/layer");
    gates:
        input in;    // SDUs from the upper mux (IP/PDCP dispatcher)
        output out;  // PDCP PDUs downward (leg dispatch is the compound's concern)
}
