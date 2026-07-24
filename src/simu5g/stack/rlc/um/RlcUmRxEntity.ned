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
// Implements the receiving-side functionalities of the Unacknowledged
// Mode (UM) of the Radio Link Control (RLC) layer for a specific connection.
// It stores RLC Protocol Data Units (PDUs) arriving from the Medium Access
// Control (MAC) layer and, when possible, performs reassembly and in-sequence
// delivery of packets to the Packet Data Convergence Protocol (PDCP) layer.
//
simple RlcUmRxEntity like simu5g.stack.rlc.IRlcRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string binderModule = default("binder");
        bool soFraming = default(false);                  // wire format: LTE FI/concat (false) vs NR SI/SO (true)
        string macModule = default("^.^.mac");  // set by the enclosing RlcUmEntityBase compound (per leg)
        string rlcMuxModule = default("^.^.rlcMux");  // this bearer's leg mux; set by the RlcUmEntityBase compound
        double timeout @unit(s) = default(1s);            // Timeout for RX Buffer (LTE mode)
        int rxWindowSize = default(16);                   // PDU reorder window (LTE mode)
        int UM_Window_Size = default(512);                // SDU reassembly window (NR mode): 16/32/512/2048
        double t_Reassembly @unit(s) = default(80ms);     // reassembly timer (NR mode)

    gates:
        input in;       // PDUs from lower mux (MAC)
        output out;     // reassembled SDUs to upper mux (PDCP)
}

