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

#ifndef __SIMU5G_NRUMRXENTITY_H_
#define __SIMU5G_NRUMRXENTITY_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"
#include "simu5g/stack/rlc/um/RlcUmReceptionBuffer.h"

namespace simu5g {

/**
 * @class NrUmRxEntity
 * @brief NR RLC UM RX entity (TS 38.322). Mux entity: RLC PDUs arrive on "in"
 * (from the MAC mux), reassembled SDUs are delivered on "out" (to the upper
 * mux/PDCP). Select via BearerManagement.rlcUmRxEntityModuleType.
 *
 * D2D mode switching and burst/throughput accounting from the original
 * standalone entity have been dropped; this entity targets the (non-D2D)
 * mux architecture.
 */
class NrUmRxEntity : public RlcRxEntityBase
{
  protected:
    RlcUmReceptionBuffer *sduBuffer = nullptr;
    int UM_Window_Size = 2048;
    omnetpp::cMessage *t_ReassemblyTimer = nullptr;
    omnetpp::simtime_t t_Reassembly;

    unsigned long totalRcvdBytes_ = 0;

    static omnetpp::simsignal_t receivedPacketFromLowerLayerSignal_;
    static omnetpp::simsignal_t sentPacketToUpperLayerSignal_;
    static omnetpp::simsignal_t rlcCellThroughputSignal_[2];

    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

    // enqueue a lower-layer PDU and try to reassemble
    void enque(inet::Packet *pkt);
    void handlePDUInReceivedBuffer(inet::Ptr<NrRlcUmDataPdu> pdu, unsigned int tsn);
    // deliver a reassembled SDU to the upper layer (PDCP) via the "out" gate
    void toPdcp(inet::Packet *rlcSdu);

  public:
    ~NrUmRxEntity() override;
};

} //namespace

#endif
