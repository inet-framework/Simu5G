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
// receiving side (TS 36.323 / 38.323 model a bidirectional entity per bearer),
// mirroring the RLC entity compounds. Created dynamically by BearerManagement,
// one per (peer, DRB) pair; a bearer with traffic in one direction only leaves
// the other side idle. The dual-connectivity gates (dcOut/dcIn to the DcMux,
// nrOut to the NR-leg RLC) are @loose and used only by the NR profile
// (~NrPdcpEntity); on an LTE entity they stay unconnected.
//
module PdcpEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/wheelbarrow");
        int headerCompressedSize @unit(B) = default(-1B);   // set per bearer by BearerManagement
        // TX and RX entity profiles, set per node/bearer by BearerManagement. They are
        // independent because an EN-DC master runs a mixed entity (NrTx for the DL split via
        // dcOut/nrOut, but LteRx); RLC never needs this, PDCP does.
        string txEntityType = default("simu5g.stack.pdcp.LteTxPdcpEntity");
        string rxEntityType = default("simu5g.stack.pdcp.LteRxPdcpEntity");
        *.headerCompressedSize = default(this.headerCompressedSize);
    gates:
        input upperIn;        // SDUs from IP (via UpperMux)
        output upperOut;      // reassembled SDUs to IP (via UpperMux)
        input lowerIn;        // PDUs from the RLC entity
        output lowerOut;      // PDUs to the RLC entity
        output dcOut @loose;  // to DcMux (NR DC, X2)
        input dcIn @loose;    // from DcMux (NR DC, X2)
        output nrOut @loose;  // to the NR-leg RLC (NR DC split bearer)
    submodules:
        tx: <txEntityType> like ITxPdcpEntity {
            @display("p=50,50");
        }
        rx: <rxEntityType> like IRxPdcpEntity {
            @display("p=150,50");
        }
    connections:
        upperIn --> { @display("m=n"); } --> tx.in;
        tx.out --> { @display("m=s"); } --> lowerOut;
        lowerIn --> { @display("m=s"); } --> rx.in;
        rx.out --> { @display("m=n"); } --> upperOut;
        tx.dcOut --> { @display("m=e"); } --> dcOut;
        dcIn --> { @display("m=e"); } --> rx.dcIn;
        tx.nrOut --> nrOut;
}
