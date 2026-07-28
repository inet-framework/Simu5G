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
// LTE (TS 36.322) receiving side of the Acknowledged Mode (AM) of the RLC
// layer: SN-contiguity defragmentation, PDU-SN reordering window. Shares the
// common RlcAmRxEntityBase shell (signal set, BearerManagement lookup for
// control-PDU routing) with the NR profile ~NrRlcAmRxEntity. Bound by default
// to BearerManagement.rlcAmRxEntityModuleType.
//
simple LteRlcAmRxEntity like simu5g.stack.rlc.IRlcAmRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string binderModule = default("binder");
        string macModule = default("^.^.mac");  // set by the enclosing RlcAmEntityBase compound (per leg)
        int rxWindowSize = default(200);
        double ackReportInterval @unit(s) = 0.10s;
        double statusReportInterval @unit(s) = 0.20s;


        //# Per-bearer statistics. A bearer serves one peer, so these are already
        //# per-UE; aggregate over a UE's bearers in the analysis tool.
        @signal[rlcDelayDl];
        @statistic[rlcDelayDl](title="Delay at the rlc layer DL"; unit="s"; source="rlcDelayDl"; record=mean);
        @signal[rlcThroughputDl];
        @statistic[rlcThroughputDl](title="Throughput at the rlc layer DL"; unit="Bps"; source="rlcThroughputDl"; record=simu5g_rateavg);
        @signal[rlcDelayUl];
        @statistic[rlcDelayUl](title="Delay at the rlc layer UL"; unit="s"; source="rlcDelayUl"; record=mean);
        @signal[rlcThroughputUl];
        @statistic[rlcThroughputUl](title="Throughput at the rlc layer UL"; unit="Bps"; source="rlcThroughputUl"; record=simu5g_rateavg);
        @signal[rlcPacketLossDl];
        @statistic[rlcPacketLossDl](title="Packet loss at the rlc layer DL"; unit=""; source="rlcPacketLossDl"; record=mean);
        @signal[rlcPacketLossUl];
        @statistic[rlcPacketLossUl](title="Packet loss at the rlc layer UL"; unit=""; source="rlcPacketLossUl"; record=mean);

    gates:
        input in;         // PDUs from lower mux (MAC)
        output out;       // reassembled SDUs to upper mux (PDCP)
        output feedbackOut;   // received STATUS PDUs -> co-located TX side (ARQ)
        output statusOut; // own STATUS reports -> co-located TX side (transmission)
}
