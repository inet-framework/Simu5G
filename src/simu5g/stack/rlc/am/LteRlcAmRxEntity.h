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

#ifndef _SIMU5G_LTERLCAMRXENTITY_H_
#define _SIMU5G_LTERLCAMRXENTITY_H_

#include <deque>

#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/timer/TTimer.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/am/RlcAmRxEntityBase.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"

namespace simu5g {

using namespace omnetpp;

class Binder;

/**
 * @class LteRlcAmRxEntity
 * @brief LTE (TS 36.322) RLC AM receiving entity.
 *
 * SN-contiguity defragmentation, PDU-SN reordering window, consumes
 * LteRlcAmPdu, emits an ACK_SN+NACK STATUS PDU.
 */
class LteRlcAmRxEntity : public RlcAmRxEntityBase
{
    inet::ModuleRefByPar<Binder> binder_;
    RlcWindowDesc rxWindowDesc_;
    simtime_t ackReportInterval_;
    simtime_t statusReportInterval_;
    int firstSdu_ = 0;
    TTimer timer_;
    cArray pduBuffer_;
    std::deque<inet::Packet *> pendingPduBuffer_;
    std::vector<bool> received_;
    std::vector<bool> discarded_;
    Direction dir_ = UNKNOWN_DIRECTION;
    static unsigned int totalCellRcvdBytes_;

  public:
    LteRlcAmRxEntity();
    ~LteRlcAmRxEntity() override;

    void enque(inet::Packet *pdu) override;
    void handleMessage(cMessage *msg) override;

  protected:

    void initMode() override;

  private:

    void passUpLte(const int index);
    void checkCompleteSdu(const int index);
    void sendStatusReportLte();
    int computeWindowShift() const;
    void moveRxWindow(const int seqNum);
    void discard(const int sn);
    inet::Packet *defragmentFrames(std::deque<inet::Packet *>& fragmentFrames);
    void routeControlToTxEntityLte(inet::Packet *pkt);
    void bufferControlViaTxEntityLte(inet::Packet *pkt);
};

} //namespace

#endif
