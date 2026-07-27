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

#ifndef __SIMU5G_NRAMTXQUEUE_H_
#define __SIMU5G_NRAMTXQUEUE_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowTransmissionBuffer.h"
#include "simu5g/stack/rlc/am/RlcSduRetransmissionBuffer.h"

namespace simu5g {

/**
 * @class NrAmTxQueue
 * @brief NR RLC AM Transmission entity (3GPP TS 38.322).
 *
 * Manages SDU segmentation, ARQ retransmissions and polling. Mux entity:
 * receives SDUs on gate "in" (from upper mux/PDCP), MAC SDU requests on
 * "macIn", and sends PDUs / new-data notifications on "out" (to the MAC).
 */
class NrAmTxQueue : public RlcTxEntityBase
{
  protected:
    struct SduInfo {
        inet::Packet *sdu = nullptr;
        int currentOffset = 0;
        ~SduInfo() { delete sdu; }
    };

    // SDU and PDU buffers
    std::list<SduInfo *> sduBuffer_;
    RlcSduSlidingWindowTransmissionBuffer *txBuffer_ = nullptr;
    RlcSduRetransmissionBuffer *rtxBuffer_ = nullptr;
    std::list<omnetpp::cPacket *> controlBuffer_;

    // Set on Radio Link Failure to stop transmission
    bool radioLinkFailureDetected_ = false;

    // Flow control info (one per logical channel)
    FlowControlInfo *lteInfo_ = nullptr;

    // Debug identifier
    std::string nameEntity_;

    // TX state variables
    unsigned int sn_ = 0;
    unsigned int txNextAck_ = 0;
    unsigned int amWindowSize_ = 0;

    // Polling state
    unsigned int pduWithoutPoll_ = 0;
    unsigned int byteWithoutPoll_ = 0;
    unsigned int pollPdu_ = 0;
    unsigned int pollByte_ = 0;
    unsigned int pollSn_ = 0;
    unsigned int maxRtxThreshold_ = 0;
    bool pollPending_ = false;
    omnetpp::cMessage *tPollRetransmitTimer_ = nullptr;
    omnetpp::simtime_t tPollRetransmit_;

    // Statistics (emitted by this entity)
    static omnetpp::simsignal_t wastedGrantedBytesSignal_;
    static omnetpp::simsignal_t enqueuedSduSizeSignal_;
    static omnetpp::simsignal_t enqueuedSduRateSignal_;
    static omnetpp::simsignal_t requestedPduSizeSignal_;
    static omnetpp::simsignal_t txWindowOccupationSignal_;
    static omnetpp::simsignal_t txWindowFullSignal_;
    static omnetpp::simsignal_t retransmissionPduSignal_;
    static omnetpp::simsignal_t receivedPacketFromUpperLayerSignal_;
    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    omnetpp::simtime_t lastSduSample_;
    unsigned int sduSampleBytes_ = 0;
    unsigned int receivedSdus_ = 0;

    void initialize(int stage) override;
    void finish() override;
    void handleMessage(omnetpp::cMessage *msg) override;

    bool sendRetransmission(int size);
    void reportBufferStatus();
    bool checkPolling();
    void sendSegment(PendingSegment segment);

    // Mux-entity send helpers (replace the old LteRlcAm wrapper calls)
    void sendPduToMac(inet::Packet *pkt);
    void sendNewDataNotification(inet::Packet *pkt);

  public:
    ~NrAmTxQueue() override;

    void enque(inet::Packet *sdu);
    void sendPdus(int size);
    void handleControlPacket(omnetpp::cPacket *pkt);
    void bufferControlPdu(omnetpp::cPacket *pkt);

    // gate-delivery variants (no Enter_Method/take: packet already owned via handleMessage)
    void processControlPacket(inet::Packet *pkt);
    void bufferControlPduInternal(inet::Packet *pkt);
    unsigned int getPendingDataVolume() const;
};

} //namespace

#endif
