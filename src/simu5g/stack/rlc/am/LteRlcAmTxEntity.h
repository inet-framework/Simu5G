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

#ifndef _SIMU5G_LTERLCAMTXENTITY_H_
#define _SIMU5G_LTERLCAMTXENTITY_H_

#include <deque>

#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/timer/TTimer.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/am/RlcAmTxEntityBase.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

/**
 * @class LteRlcAmTxEntity
 * @brief LTE (TS 36.322) RLC AM transmission entity.
 *
 * Fixed-unit fragmentation of SDUs, whole-PDU ARQ retransmission driven by a
 * per-PDU TMultiTimer, emits LteRlcAmPdu.
 */
class LteRlcAmTxEntity : public RlcAmTxEntityBase
{
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

  public:
    LteRlcAmTxEntity();
    ~LteRlcAmTxEntity() override;

    void enque(Packet *sdu) override;
    void sendPdus(int size) override;
    void bufferControlPdu(cPacket *pkt) override;
    void handleControlPacket(cPacket *pkt) override;
    unsigned int getPendingDataVolume() const override;

  protected:

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;

  private:

    void addPdus();
    void discard(int seqNum);
    void bufferPdu(cPacket *pdu);

    // gate-delivery variants (no Enter_Method/take: packet already owned via handleMessage)
    void bufferPduInternal(inet::Packet *pdu);
    void processControlPacket(inet::Packet *pdu);
    void sendNewDataNotificationLte(inet::Packet *pkt);
    void moveTxWindow(const int seqNum);
    void advanceTxWindow();
    void recvCumulativeAck(const int seqNum);
    void recvAck(const int seqNum);
    void pduTimerHandle(const int sn);
    std::deque<Packet *> *fragmentFrame(Packet *frame, std::deque<int>& windowsIndex, RlcFragDesc rlcFragDesc);
};

} //namespace

#endif
