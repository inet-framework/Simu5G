//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.rlc.tm;

//
// Implements the receiving side of the Transparent Mode (TM) of the
// Radio Link Control (RLC) layer for a specific connection.
// Simply forwards received PDUs to the upper layer without
// reassembly or reordering.
//
simple RlcTmRxEntity like simu5g.stack.rlc.IRlcRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");

    gates:
        input in;       // PDUs from lower mux (MAC)
        output out;     // SDUs to upper mux (PDCP)
}
