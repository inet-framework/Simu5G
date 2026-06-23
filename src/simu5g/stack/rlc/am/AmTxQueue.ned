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
// Generic transmit side of RLC AM, parametrized for LTE or NR via soFraming
// (one C++ class, AmTxQueue):
//  - LTE (soFraming=false, TS 36.322): fixed-unit fragmentation, whole-PDU ARQ retx.
//  - NR  (soFraming=true,  TS 38.322): SO byte-offset segmentation + re-segment, polling.
// The NR parametrization is provided by the NrAmTxQueue profile. Select via
// BearerManagement.rlcAmTxEntityModuleType.
//
simple AmTxQueue like simu5g.stack.rlc.IRlcAmTxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        bool soFraming = default(false);                  // wire format: LTE frag (false) vs NR SO (true)

        // LTE (fragment / whole-PDU ARQ) parameters
        int fragmentSize @unit(B) = default(30B);         // Size of fragments
        int maxRtx = default(3);
        double pduRtxTimeout @unit(s) = default(2.0s);
        double ctrlPduRtxTimeout @unit(s) = default(2.0s);
        double bufferStatusTimeout @unit(s) = default(2.0s);
        int txWindowSize = default(200);

        // NR (SO segmentation + polling) parameters
        int AM_Window_Size = default(512);                // 512 (10-bit) / 2048 (12-bit) / 131072 (18-bit)
        int pollByte @unit(B) = default(125kB);
        int pollPDU = default(64);
        int maxRtxThreshold = default(4);
        double t_PollRetransmit @unit(s) = default(80ms);

    gates:
        input in;       // SDUs from upper mux (PDCP)
        input macIn;    // SDU requests from lower mux (MAC)
        output out;     // PDUs and new-data notifications to lower mux (MAC)
        input feedbackIn;   // STATUS PDUs received from the peer, from the co-located RX side (to ARQ)
        input statusIn; // locally generated STATUS reports, from the co-located RX side (to transmit)
}
