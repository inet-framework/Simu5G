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

package simu5g.stack.rlc;

//
// Module interface for the receiving side of an RLC entity (TM/UM/AM).
// Concrete entity types (e.g. TmRxEntity, UmRxEntity, AmRxQueue) implement
// this interface so that RlcTmEntity/RlcUmEntityBase/RlcAmEntityBase compound
// modules can instantiate them via a typename parameter. Implementations
// may declare additional gates (e.g. the AM control gates).
//
moduleinterface IRlcRxEntity
{
    parameters:
        @display("i=block/layer");
    gates:
        input in;       // PDUs from lower mux (MAC)
        output out;     // reassembled SDUs to upper mux (PDCP)
}
