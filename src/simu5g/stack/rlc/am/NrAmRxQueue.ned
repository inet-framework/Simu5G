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
// NR RLC AM RX entity (TS 38.322). Plugs into the RLC mux architecture as the
// AM RX entity (select via BearerManagement.rlcAmRxEntityModuleType). Inherits
// the gates (in/out) from AmRxQueue.
//
simple NrAmRxQueue extends AmRxQueue
{
    parameters:
        @class(NrAmRxQueue);
        int AM_Window_Size = default(2048);   // 2048 (12-bit SN) or 131072 (18-bit SN)
        double t_Reassembly @unit(s) = default(80ms);
        double t_StatusProhibit @unit(s) = default(100ms);

        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToUpperLayer];
        @statistic[sentPacketToUpperLayer](source="sentPacketToUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[rxWindowOccupation];
        @statistic[rxWindowOccupation](title="RX window occupation"; source="rxWindowOccupation"; record=mean,vector);
        @signal[rlcCellThroughputDl];
        @statistic[rlcCellThroughputDl](title="Cell Throughput at the RLC layer DL"; unit="Bps"; source="rlcCellThroughputDl"; record=simu5g_rateavg);
        @signal[rlcCellThroughputUl];
        @statistic[rlcCellThroughputUl](title="Cell Throughput at the RLC layer UL"; unit="Bps"; source="rlcCellThroughputUl"; record=simu5g_rateavg);
}
