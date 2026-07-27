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
// NR RLC UM TX entity (TS 38.322). Plugs into the RLC mux architecture as the
// UM TX entity (select via BearerManagement.rlcUmTxEntityModuleType). Inherits
// the gates (in/macIn/out) from UmTxEntity.
//
simple NrUmTxEntity extends UmTxEntity
{
    parameters:
        @class(NrUmTxEntity);
        int sn_FieldLength = default(12);   // UM SN field length in bits: 6 or 12

        @signal[wastedGrantedBytes];
        @statistic[wastedGrantedBytes](title="Wasted granted bytes"; unit="B"; source="wastedGrantedBytes"; record=sum);
        @signal[requestedPDUSize];
        @statistic[requestedPDUSize](title="Requested PDU size"; unit="B"; source="requestedPDUSize"; record=vector);
        @signal[sentPDUSize];
        @statistic[sentPDUSize](title="Sent PDU size"; unit="B"; source="sentPDUSize"; record=vector);
}
