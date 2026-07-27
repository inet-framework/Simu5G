//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_AMTXBUFFER_H_
#define _LTE_AMTXBUFFER_H_

#include <list>

#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/timer/TTimer.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowTransmissionBuffer.h"
#include "simu5g/stack/rlc/am/RlcSduRetransmissionBuffer.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

/*
 * Generic RLC AM Transmission Entity, parametrized for LTE or NR via soFraming:
 *  - LTE (soFraming=false, TS 36.322): fixed-unit fragmentation, whole-PDU ARQ
 *    retransmission with a per-PDU TMultiTimer, emits LteRlcAmPdu.
 *  - NR  (soFraming=true,  TS 38.322): byte-offset (SO) segmentation + re-segment
 *    on retransmission via a task-queue, polling, emits NrRlcAmDataPdu.
 * The MAC plumbing and the ACK_SN+NACK STATUS format are shared; the ARQ engine
 * dispatches on the mode.
 */
class AmTxQueue : public RlcTxEntityBase
{
  protected:

    // --- wire-format selector (profile-driven; see UmTxEntity) ---
    bool soFraming_ = false;

    // Copy of the flow control info (shared by both modes).
    FlowControlInfo *lteInfo_ = nullptr;

    // --- LTE (fragment/whole-PDU ARQ) state ---
    Packet *currentSdu_ = nullptr;
    std::deque<Packet *> *fragmentList_ = nullptr;
    std::deque<int> txWindowIndexList_;
    RlcFragDesc fragDesc_;
    cPacketQueue sduQueue_;
    cArray pduRtxQueue_;
    inet::cPacketQueue pduBuffer_;
    std::vector<bool> received_;
    std::vector<bool> discarded_;
    RlcWindowDesc txWindowDesc_;
    TMultiTimer pduTimer_;
    TTimer bufferStatusTimer_;
    int maxRtx_;
    simtime_t pduRtxTimeout_;
    simtime_t ctrlPduRtxTimeout_;
    simtime_t bufferStatusTimeout_;

    // --- NR (SO segmentation + polling) state ---
    struct SduInfo {
        inet::Packet *sdu = nullptr;
        int currentOffset = 0;
        ~SduInfo() { delete sdu; }
    };
    std::list<SduInfo *> sduBuffer_;
    RlcSduSlidingWindowTransmissionBuffer *txBuffer_ = nullptr;
    RlcSduRetransmissionBuffer *rtxBuffer_ = nullptr;
    std::list<omnetpp::cPacket *> controlBuffer_;
    bool radioLinkFailureDetected_ = false;
    std::string nameEntity_;
    unsigned int sn_ = 0;
    unsigned int txNextAck_ = 0;
    unsigned int amWindowSize_ = 0;
    unsigned int pduWithoutPoll_ = 0;
    unsigned int byteWithoutPoll_ = 0;
    unsigned int pollPdu_ = 0;
    unsigned int pollByte_ = 0;
    unsigned int pollSn_ = 0;
    unsigned int maxRtxThreshold_ = 0;
    bool pollPending_ = false;
    omnetpp::cMessage *tPollRetransmitTimer_ = nullptr;
    omnetpp::simtime_t tPollRetransmit_;
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

  public:
    AmTxQueue();
    ~AmTxQueue() override;

    // Enqueue an upper-layer SDU into the transmission buffer
    void enque(Packet *sdu);

    // Send buffered PDUs until the requested grant size is reached
    void sendPdus(int size);

    // Buffer a control (STATUS) PDU for transmission
    void bufferControlPdu(cPacket *pkt);

    // Receive a control (STATUS) message from the AM receiver
    virtual void handleControlPacket(cPacket *pkt);

    // NR: pending data volume reported to MAC
    unsigned int getPendingDataVolume() const;

  protected:

    void initialize(int stage) override;
    void finish() override;
    void handleMessage(cMessage *msg) override;

  private:

    // --- LTE (fragment/whole-PDU ARQ) implementation ---
    void enqueLte(Packet *sdu);
    void addPdus();
    void sendPdusLte(int size);
    void bufferControlPduLte(cPacket *pkt);
    void handleControlPacketLte(cPacket *pkt);
    void discard(int seqNum);
    void bufferPdu(cPacket *pdu);

    // gate-delivery variants (no Enter_Method/take: packet already owned via handleMessage)
    void bufferPduInternal(inet::Packet *pdu);
    void processControlPacketLte(inet::Packet *pdu);
    void processControlPacketNr(inet::Packet *pdu);
    void bufferControlPduNrInternal(inet::Packet *pdu);

    void sendNewDataNotificationLte(inet::Packet *pkt);

    /* Move the transmitter window based upon the reception of an ACK control message
     *
     * @param seqNum
     */
    void moveTxWindow(const int seqNum);
    void advanceTxWindow();
    void recvCumulativeAck(const int seqNum);
    void recvAck(const int seqNum);
    void pduTimerHandle(const int sn);
    std::deque<Packet *> *fragmentFrame(Packet *frame, std::deque<int>& windowsIndex, RlcFragDesc rlcFragDesc);

    // --- NR (SO segmentation + polling) implementation ---
    void enqueNr(Packet *sdu);
    void sendPdusNr(int size);
    void bufferControlPduNr(cPacket *pkt);
    void handleControlPacketNr(cPacket *pkt);
    bool sendRetransmission(int size);
    void reportBufferStatus();
    bool checkPolling();
    void sendSegment(PendingSegment segment);
    void sendPduToMac(inet::Packet *pkt);
    void sendNewDataNotificationNr(inet::Packet *pkt);
};

} //namespace

#endif
