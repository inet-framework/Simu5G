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

package simu5g.stack.rlc.um;

//
// NR RLC UM RX entity (TS 38.322). Plugs into the RLC mux architecture as the
// UM RX entity (select via BearerManagement.rlcUmRxEntityModuleType). Inherits
// the gates (in/out) from UmRxEntity.
//
simple NrUmRxEntity extends UmRxEntity
{
    parameters:
        @class(NrUmRxEntity);
        int UM_Window_Size = default(2048);     // 2048 (12-bit SN) or 32 (6-bit SN)
        double t_Reassembly @unit(s) = default(80ms);

        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToUpperLayer];
        @statistic[sentPacketToUpperLayer](source="sentPacketToUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[rlcCellThroughputDl];
        @statistic[rlcCellThroughputDl](title="Cell Throughput at the RLC layer DL"; unit="Bps"; source="rlcCellThroughputDl"; record=simu5g_rateavg);
        @signal[rlcCellThroughputUl];
        @statistic[rlcCellThroughputUl](title="Cell Throughput at the RLC layer UL"; unit="Bps"; source="rlcCellThroughputUl"; record=simu5g_rateavg);
}
