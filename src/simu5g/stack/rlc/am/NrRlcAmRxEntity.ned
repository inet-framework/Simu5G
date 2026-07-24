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
// NR profile of the generic AM RX entity (TS 38.322): same C++ class (RlcAmRxEntity)
// as the LTE entity, parametrized with soFraming=true and the NR window/timers.
// Select via BearerManagement.rlcAmRxEntityModuleType. The NR SO-reassembly
// statistics are declared here so they are recorded only for NR-selected entities
// (LTE entities keep their original scalar set).
//
simple NrRlcAmRxEntity extends RlcAmRxEntity
{
    parameters:
        @class(RlcAmRxEntity);
        soFraming = default(true);            // select the NR SO byte-offset wire format
        AM_Window_Size = default(2048);       // 2048 (12-bit SN) or 131072 (18-bit SN)
        t_Reassembly = default(80ms);
        t_StatusProhibit = default(100ms);

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
