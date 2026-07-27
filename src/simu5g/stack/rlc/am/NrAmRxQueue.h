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

#ifndef __SIMU5G_NRAMRXQUEUE_H_
#define __SIMU5G_NRAMRXQUEUE_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowReceptionBuffer.h"

namespace simu5g {

class BearerManagement;
class RlcMux;

/**
 * @class NrAmRxQueue
 * @brief NR RLC AM Reception entity (3GPP TS 38.322).
 *
 * Reassembles SDUs and generates STATUS reports. Mux entity: receives PDUs on
 * gate "in" (from the MAC via the mux) and delivers reassembled SDUs on "out"
 * (to the upper mux/PDCP). STATUS/ACK control PDUs are routed to the local AM
 * TX entity (looked up via BearerManagement).
 */
class NrAmRxQueue : public RlcRxEntityBase
{
  public:
    ~NrAmRxQueue() override;

    void enque(inet::Packet *pkt);

  protected:
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

    void passUp(int seqNum);
    void sendStatusReport();

    //! Route an incoming control PDU (ACK) to the corresponding AM TX entity
    void routeControlToTxEntity(inet::Packet *pkt);
    //! Buffer an outgoing STATUS PDU via the corresponding AM TX entity for transmission
    void bufferControlViaTxEntity(inet::Packet *pkt);


    // FlowControlInfo for building status reports (with reversed directions)
    FlowControlInfo *ackFlowControlInfo_ = nullptr;

    // Debug identifier
    std::string nameEntity_;

    // Statistics (emitted by this entity)
    static omnetpp::simsignal_t receivedPacketFromLowerLayerSignal_;
    static omnetpp::simsignal_t sentPacketToUpperLayerSignal_;
    static omnetpp::simsignal_t rxWindowOccupationSignal_;
    static omnetpp::simsignal_t rlcCellThroughputSignal_[2];   // [DL, UL]
    unsigned int totalRcvdBytes_ = 0;

    // Reassembly buffer
    RlcSduSlidingWindowReceptionBuffer *rxBuffer_ = nullptr;
    std::set<unsigned int> passedUpSdus_;

    // Timers
    omnetpp::cMessage *tReassemblyTimer_ = nullptr;
    omnetpp::simtime_t tReassembly_;
    omnetpp::cMessage *tStatusProhibitTimer_ = nullptr;
    omnetpp::simtime_t tStatusProhibit_;
    omnetpp::simtime_t lastSentAck_;

    // RX state
    unsigned int rxNextStatusTrigger_ = 0;
    unsigned int amWindowSize_ = 0;
    bool statusReportPending_ = false;
};

} //namespace

#endif
