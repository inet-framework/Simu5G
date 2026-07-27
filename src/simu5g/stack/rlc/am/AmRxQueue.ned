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
// Generic receive side of RLC AM, parametrized for LTE or NR via soFraming
// (one C++ class, AmRxQueue):
//  - LTE (soFraming=false, TS 36.322): SN-contiguity defragmentation, PDU-SN window.
//  - NR  (soFraming=true,  TS 38.322): SO byte-coverage reassembly, SDU-SN window,
//    t-Reassembly + t-StatusProhibit.
// The NR parametrization is provided by the NrAmRxQueue profile. Select via
// BearerManagement.rlcAmRxEntityModuleType.
//
simple AmRxQueue like simu5g.stack.rlc.IRlcAmRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string binderModule = default("binder");
        bool isNR @mutable = default(false);              // NR-leg marker (set programmatically by BearerManagement; not the wire format)
        bool soFraming = default(false);                  // wire format: LTE frag (false) vs NR SO (true)
        string upperMuxModule = default("^.upperMux");
        string macModule = default("^.^.mac");  // set by the enclosing RlcAmEntityBase compound (per leg)
        string rlcMuxModule = default("^.^.rlcMux");  // this bearer's leg mux

        // LTE (fragment / SN-contiguity) parameters
        int rxWindowSize = default(200);
        double ackReportInterval @unit(s) = 0.10s;
        double statusReportInterval @unit(s) = 0.20s;
        double timeout @unit(s) = default(1s);            // Timeout for RX Buffer

        // NR (SO byte-coverage) parameters
        int AM_Window_Size = default(512);                // 512 (10-bit) / 2048 (12-bit) / 131072 (18-bit)
        double t_Reassembly @unit(s) = default(80ms);
        double t_StatusProhibit @unit(s) = default(100ms);

    gates:
        input in;        // PDUs from lower mux (MAC)
        output out;      // reassembled SDUs to upper mux (PDCP)
        output feedbackOut;   // received STATUS PDUs -> co-located TX side (ARQ)
        output statusOut; // own STATUS reports -> co-located TX side (transmission)
}
