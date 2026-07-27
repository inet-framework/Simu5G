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

#ifndef __SIMU5G_NRUMTXENTITY_H_
#define __SIMU5G_NRUMTXENTITY_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/um/RlcUmTransmitterBuffer.h"

namespace simu5g {

/**
 * @class NrUmTxEntity
 * @brief NR RLC UM TX entity (TS 38.322). Mux entity: SDUs arrive on "in"
 * (from the upper mux/PDCP), MAC SDU requests on "macIn", and PDUs / new-data
 * notifications go out on "out" (to the MAC). Select via
 * BearerManagement.rlcUmTxEntityModuleType.
 */
class NrUmTxEntity : public RlcTxEntityBase
{
  protected:
    RlcUmTransmitterBuffer *sduBuffer = nullptr;
    unsigned int sn_FieldLength = 12;

    static omnetpp::simsignal_t wastedGrantedBytes;
    static omnetpp::simsignal_t requestedPDUSizeSignal;
    static omnetpp::simsignal_t sentPDUSizeSignal;

    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

    void handleSdu(inet::Packet *pkt);
    void rlcPduMake(int pduSize);
    void sendPduToMac(inet::Packet *pkt);

  public:
    ~NrUmTxEntity() override;
};

} //namespace

#endif
