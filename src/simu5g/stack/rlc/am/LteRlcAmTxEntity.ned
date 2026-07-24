//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//


package simu5g.stack.rlc.am;

//
// LTE (TS 36.322) transmission side of the Acknowledged Mode (AM) of the RLC
// layer: fixed-unit fragmentation of SDUs, whole-PDU ARQ retransmission driven
// by a per-PDU timer. Shares the common RlcAmTxEntityBase shell (signal set,
// flow-control-info bookkeeping) with the NR profile ~NrRlcAmTxEntity. Bound
// by default to BearerManagement.rlcAmTxEntityModuleType.
//
simple LteRlcAmTxEntity like simu5g.stack.rlc.IRlcAmTxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        int fragmentSize @unit(B) = default(30B);         // Size of fragments
        int maxRtx = default(3);
        double pduRtxTimeout @unit(s) = default(2.0s);
        double ctrlPduRtxTimeout @unit(s) = default(2.0s);
        double bufferStatusTimeout @unit(s) = default(2.0s);
        int txWindowSize = default(200);

    gates:
        input in;       // SDUs from upper mux (PDCP)
        input macIn;    // SDU requests from lower mux (MAC)
        output out;     // PDUs and new-data notifications to lower mux (MAC)
        input feedbackIn;   // STATUS PDUs received from the peer, from the co-located RX side (to ARQ)
        input statusIn; // locally generated STATUS reports, from the co-located RX side (to transmit)
}
