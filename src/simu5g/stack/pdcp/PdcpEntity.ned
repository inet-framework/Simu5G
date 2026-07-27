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
// PDCP entity of one data radio bearer: packages the transmitting and the
// receiving side of TS 36.323 / 38.323, plus the leg split/join stage of a
// multi-leg bearer (e.g. an NR dual connectivity split bearer). Created
// dynamically by BearerManagement, one per (peer, DRB) pair.
//
// The lower boundary is a vector of legs. A plain bearer has one leg, and the
// TX/RX entities connect straight to it; a multi-leg bearer routes the TX side
// through a splitter (per-PDU leg choice and per-leg id mapping) and merges
// all legs through a joiner into the single RX entity, whose one reorder
// window restores SN order across legs. BearerManagement wires each leg to a
// local RLC entity (UE legs, gNB local leg) or to the DcMux/X2 path (a DC
// master's remote leg).
//
// The legs parameter holds one descriptor per leg, e.g.
//   [ {rat: "lte"}, {rat: "nr"} ]    <- UE: local LTE + local NR stack
//   [ {rat: "lte"}, {rat: "x2"} ]    <- DC master: local + remote via X2
// set by BearerManagement on creation; the "rat" field selects the splitter's
// per-leg id mapping (see ~PdcpLegSplitter).
//
// Variants are subclasses overriding the entity typenames (e.g. ~NrPdcpEntity;
// a mixed entity can override just tx.typename or rx.typename from the
// configuration).
//
module PdcpEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/wheelbarrow");
        int numLegs = default(1);                  // number of RLC legs of this bearer
        int headerCompressedSize @unit(B) = default(-1B);
        object legs = default([]);                 // per-leg descriptors (see module comment)
        *.headerCompressedSize = default(this.headerCompressedSize);
        *.numLegs = this.numLegs;
        *.legs = dup(this.legs);
        *.emitPerSduSignals = this.numLegs == 1;   // on a multi-leg bearer, the splitter emits the per-leg statistics
        *.reorderingEnabled = this.numLegs > 1;    // reorder the SN stream merged from several legs
    gates:
        input upperIn;            // SDUs from IP (via UpperMux)
        output upperOut;          // reassembled SDUs to IP (via UpperMux)
        output legOut[numLegs];   // PDUs to leg k (RLC entity, or DcMux/X2)
        input legIn[numLegs];     // PDUs from leg k
    submodules:
        tx: <default("simu5g.stack.pdcp.LteTxPdcpEntity")> like ITxPdcpEntity {
            @display("p=50,50");
        }
        rx: <default("simu5g.stack.pdcp.LteRxPdcpEntity")> like IRxPdcpEntity {
            @display("p=150,50");
        }
        splitter: <default("simu5g.stack.pdcp.PdcpLegSplitter")> like IPdcpLegSplitter if numLegs > 1 {
            @display("p=50,130");
        }
        joiner: <default("simu5g.stack.pdcp.PdcpLegJoiner")> like IPdcpLegJoiner if numLegs > 1 {
            @display("p=150,130");
        }
    connections:
        upperIn --> { @display("m=n"); } --> tx.in;
        rx.out --> { @display("m=n"); } --> upperOut;
        // single-leg bearer: entities connect straight to the leg
        tx.out --> { @display("m=s"); } --> legOut[0] if numLegs == 1;
        legIn[0] --> { @display("m=s"); } --> rx.in if numLegs == 1;
        // multi-leg bearer: fan-out / fan-in stages
        tx.out --> splitter.in if numLegs > 1;
        joiner.out --> rx.in if numLegs > 1;
        for i=0..numLegs-1 {
            splitter.out++ --> { @display("m=s"); } --> legOut[i] if numLegs > 1;
            legIn[i] --> { @display("m=s"); } --> joiner.in++ if numLegs > 1;
        }
}
