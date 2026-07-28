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
// Receive Buffer of RLC AM
//
simple AmRxQueue like simu5g.stack.rlc.IRlcAmRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string binderModule = default("binder");
        bool isNR @mutable = default(false);  // set programmatically by BearerManagement
        string upperMuxModule = default("^.upperMux");
        string macModule = default("^.^.mac");  // set by the enclosing RlcAmEntity compound (per leg)
        int rxWindowSize = default(200);
        double ackReportInterval @unit(s) = 0.10s;
        double statusReportInterval @unit(s) = 0.20s;
        double timeout @unit(s) = default(1s);            // Timeout for RX Buffer

    gates:
        input in;        // PDUs from lower mux (MAC)
        output out;      // reassembled SDUs to upper mux (PDCP)
        output feedbackOut;   // received STATUS PDUs -> co-located TX side (ARQ)
        output statusOut; // own STATUS reports -> co-located TX side (transmission)
}

//
// Entities for the RLC module
//

