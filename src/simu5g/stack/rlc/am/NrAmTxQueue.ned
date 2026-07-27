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
// NR RLC AM TX entity (TS 38.322). Plugs into the RLC mux architecture as the
// AM TX entity (select via BearerManagement.rlcAmTxEntityModuleType). Inherits
// the gates (in/macIn/out) from AmTxQueue.
//
simple NrAmTxQueue extends AmTxQueue
{
    parameters:
        @class(NrAmTxQueue);
        int AM_Window_Size = default(2048);   // 2048 (12-bit SN) or 131072 (18-bit SN)
        int pollByte @unit(B) = default(125kB);
        int pollPDU = default(64);
        int maxRtxThreshold = default(4);
        double t_PollRetransmit @unit(s) = default(80ms);

        @signal[receivedPacketFromUpperLayer];
        @statistic[receivedPacketFromUpperLayer](source="receivedPacketFromUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToLowerLayer];
        @statistic[sentPacketToLowerLayer](source="sentPacketToLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[txWindowOccupation];
        @statistic[txWindowOccupation](title="TX window occupation"; source="txWindowOccupation"; record=mean,vector);
        @signal[wastedGrantedBytes];
        @statistic[wastedGrantedBytes](title="Wasted granted bytes"; unit="B"; source="wastedGrantedBytes"; record=sum);
        @signal[requestedPduSize];
        @statistic[requestedPduSize](title="Requested PDU size"; unit="B"; source="requestedPduSize"; record=vector);
        @signal[enqueuedSduSize];
        @statistic[enqueuedSduSize](title="Enqueued SDU size"; unit="B"; source="enqueuedSduSize"; record=vector);
        @signal[enqueuedSduRate];
        @statistic[enqueuedSduRate](title="Enqueued SDU rate"; source="enqueuedSduRate"; record=vector);
        @signal[txWindowFull];
        @statistic[txWindowFull](title="TX window full events"; source="txWindowFull"; record=count);
        @signal[retransmissionPdu];
        @statistic[retransmissionPdu](title="Retransmitted PDUs"; source="retransmissionPdu"; record=count);
}
