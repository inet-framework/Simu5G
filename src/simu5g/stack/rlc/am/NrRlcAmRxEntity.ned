//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

package simu5g.stack.rlc.am;

//
// NR (TS 38.322) receiving side of the Acknowledged Mode (AM) of the RLC
// layer: SO byte-coverage reassembly with a per-SDU SN window, t-Reassembly +
// t-StatusProhibit timers. Shares the common RlcAmRxEntityBase shell with the
// LTE profile ~LteRlcAmRxEntity. Bound by default to
// BearerManagement.nrRlcAmEntityModuleType. The NR SO-reassembly statistics
// are declared here so they are recorded only for NR-selected entities.
//
simple NrRlcAmRxEntity like simu5g.stack.rlc.IRlcAmRxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        int AM_Window_Size = default(2048);       // 2048 (12-bit SN) or 131072 (18-bit SN)
        double t_Reassembly @unit(s) = default(80ms);
        double t_StatusProhibit @unit(s) = default(100ms);

        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToUpperLayer];
        @statistic[sentPacketToUpperLayer](source="sentPacketToUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[rxWindowOccupation];
        @statistic[rxWindowOccupation](title="RX window occupation"; source="rxWindowOccupation"; record=mean,vector);


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
        @signal[rlcPduDelayDl];
        @statistic[rlcPduDelayDl](title="PDU delay at the rlc layer DL"; unit="s"; source="rlcPduDelayDl"; record=mean);
        @signal[rlcPduThroughputDl];
        @statistic[rlcPduThroughputDl](title="PDU throughput at the rlc layer DL"; unit="Bps"; source="rlcPduThroughputDl"; record=simu5g_rateavg);
        @signal[rlcPduDelayUl];
        @statistic[rlcPduDelayUl](title="PDU delay at the rlc layer UL"; unit="s"; source="rlcPduDelayUl"; record=mean);
        @signal[rlcPduThroughputUl];
        @statistic[rlcPduThroughputUl](title="PDU throughput at the rlc layer UL"; unit="Bps"; source="rlcPduThroughputUl"; record=simu5g_rateavg);
        @signal[rlcPacketLossDl];
        @statistic[rlcPacketLossDl](title="Packet loss at the rlc layer DL"; unit=""; source="rlcPacketLossDl"; record=mean);
        @signal[rlcPacketLossUl];
        @statistic[rlcPacketLossUl](title="Packet loss at the rlc layer UL"; unit=""; source="rlcPacketLossUl"; record=mean);

    gates:
        input in;       // PDUs from lower mux (MAC)
        output out;       // reassembled SDUs to upper mux (PDCP)
        output feedbackOut;   // received STATUS PDUs -> co-located TX side (ARQ)
        output statusOut; // own STATUS reports -> co-located TX side (transmission)
}
