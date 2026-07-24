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


package simu5g.stack.rlc.um;

//
// LTE (TS 36.322) receiving side of the Unacknowledged Mode (UM) of the RLC
// layer: FI-walk reassembly across concatenated PDUs with a PDU-SN reordering
// window and a t-Reordering timer. Shares the common RlcUmRxEntityBase shell
// (MAC-mux plumbing, D2D hooks, UL burst-throughput accounting) with the NR
// profile ~NrRlcUmRxEntity. Bound by default to
// BearerManagement.rlcUmRxEntityModuleType. Its throughput/delay statistics are
// emitted on the sibling rlcMux module (see ~RlcMux).
//
simple LteRlcUmRxEntity like simu5g.stack.rlc.IRlcRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string binderModule = default("binder");
        string macModule = default("^.^.mac");  // set by the enclosing RlcUmEntityBase compound (per leg)
        string rlcMuxModule = default("^.^.rlcMux");  // this bearer's leg mux; set by the RlcUmEntityBase compound
        double timeout @unit(s) = default(1s);            // Timeout for RX Buffer
        int rxWindowSize = default(16);                   // PDU reorder window

    gates:
        input in;       // PDUs from lower mux (MAC)
        output out;     // reassembled SDUs to upper mux (PDCP)
}
