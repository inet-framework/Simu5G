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
// NR (TS 38.322) transmission side of the Unacknowledged Mode (UM) of the RLC
// layer: SI + byte-offset (SO) segmentation, one SDU segment per PDU, one sequence
// number per SDU. Shares the common RlcUmTxEntityBase shell (MAC plumbing, D2D
// mode-switch machinery) with the LTE profile ~LteRlcUmTxEntity. Instantiated as the tx side of the
// NrRlc*Entity compound (BearerManagement.nrRlcUmEntityModuleType). The NR SO-segmentation statistics
// are declared here so they are recorded only for NR entities.
//
simple NrRlcUmTxEntity like simu5g.stack.rlc.IRlcTxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        string macModule = default("^.^.mac");  // set by the enclosing RlcUmEntityBase compound (per leg)
        int sn_FieldLength = default(12);   // UM SN field length in bits: 6 or 12

        @signal[wastedGrantedBytes];
        @statistic[wastedGrantedBytes](title="Wasted granted bytes"; unit="B"; source="wastedGrantedBytes"; record=sum);
        @signal[requestedPDUSize];
        @statistic[requestedPDUSize](title="Requested PDU size"; unit="B"; source="requestedPDUSize"; record=vector);
        @signal[sentPDUSize];
        @statistic[sentPDUSize](title="Sent PDU size"; unit="B"; source="sentPDUSize"; record=vector);

    gates:
        input in;       // SDUs from upper mux (PDCP)
        input macIn;    // SDU requests from lower mux (MAC)
        output out;     // PDUs and new-data notifications to lower mux (MAC)
}
