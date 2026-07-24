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
// NR profile of the generic UM TX entity (TS 38.322): same C++ class
// (RlcUmTxEntity) as the LTE entity, parametrized with isNR=true and the NR SN
// field length. Select via BearerManagement.rlcUmTxEntityModuleType. The NR
// SO-segmentation statistics are declared here so they are recorded only for
// NR-selected entities (LTE entities keep their original scalar set).
//
simple NrRlcUmTxEntity extends RlcUmTxEntity
{
    parameters:
        @class(RlcUmTxEntity);
        soFraming = default(true);      // select the NR SI/SO byte-offset wire format
        sn_FieldLength = default(12);   // UM SN field length in bits: 6 or 12

        @signal[wastedGrantedBytes];
        @statistic[wastedGrantedBytes](title="Wasted granted bytes"; unit="B"; source="wastedGrantedBytes"; record=sum);
        @signal[requestedPDUSize];
        @statistic[requestedPDUSize](title="Requested PDU size"; unit="B"; source="requestedPDUSize"; record=vector);
        @signal[sentPDUSize];
        @statistic[sentPDUSize](title="Sent PDU size"; unit="B"; source="sentPDUSize"; record=vector);
}
