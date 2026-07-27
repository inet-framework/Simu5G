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
// RLC AM entity of one data radio bearer: packages the transmitting and the
// receiving side into the single bidirectional entity of TS 36.322 / 38.322
// section 4.2.1. Created dynamically by BearerManagement, one per (peer, DRB)
// pair.
//
// The receiving side hands control PDUs to the co-located transmitting side
// over two internal connections:
//  - ctrl: STATUS PDUs received from the peer, consumed by the TX side's ARQ
//    (acknowledgements for data this entity transmitted);
//  - status: locally generated STATUS reports, which the TX side transmits to
//    the peer on this bearer's logical channel (data and control PDUs share
//    the logical channel in AM).
//
module RlcAmEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/wheelbarrow");
        string txEntityType = default("simu5g.stack.rlc.am.AmTxQueue");
        string rxEntityType = default("simu5g.stack.rlc.am.AmRxQueue");
    gates:
        input upperIn;    // SDUs from PDCP
        output upperOut;  // reassembled SDUs to PDCP
        input lowerIn;    // PDUs from RlcMux (data + control)
        output lowerOut;  // PDUs and new-data notifications to RlcMux/MAC
        input macIn;      // MAC SDU requests via RlcMux
    submodules:
        tx: <txEntityType> like IRlcAmTxEntity {
            @display("p=50,50");
        }
        rx: <rxEntityType> like IRlcAmRxEntity {
            @display("p=150,50");
        }
    connections:
        upperIn --> { @display("m=n"); } --> tx.in;
        macIn --> { @display("m=s"); } --> tx.macIn;
        tx.out --> { @display("m=s"); } --> lowerOut;
        lowerIn --> { @display("m=s"); } --> rx.in;
        rx.out --> { @display("m=n"); } --> upperOut;
        rx.ctrlOut --> tx.ctrlIn;      // received STATUS PDUs -> TX-side ARQ
        rx.statusOut --> tx.statusIn;  // own STATUS reports -> TX for transmission
}
