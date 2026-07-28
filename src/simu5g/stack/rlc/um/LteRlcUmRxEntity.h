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

#ifndef _SIMU5G_LTERLCUMRXENTITY_H_
#define _SIMU5G_LTERLCUMRXENTITY_H_


#include "simu5g/stack/rlc/um/RlcUmRxEntityBase.h"
#include "simu5g/common/timer/TTimer.h"

namespace simu5g {

using namespace omnetpp;

class LteMacBase;
class LteRlcUmDataPdu;

/**
 * @class LteRlcUmRxEntity
 * @brief LTE (TS 36.322) RLC UM receiving entity.
 *
 * FI-walk reassembly across concatenated PDUs, PDU-SN reordering window with a
 * t-Reordering timer. Delivers in-sequence SDUs to PDCP.
 */
class LteRlcUmRxEntity : public RlcUmRxEntityBase
{
    cArray pduBuffer_;
    RlcUmRxWindowDesc rxWindowDesc_;
    TTimer t_reordering_;
    double timeout_;
    std::vector<bool> received_;
    struct Buffered {
        inet::Packet *pkt = nullptr;
        size_t size = 0;
        unsigned int currentPduSno = 0;
    } buffered_;
    unsigned int lastPduReassembled_ = 0;
    bool init_ = false;
    unsigned int totalPduRcvdBytes_ = 0;
    unsigned int totalRcvdBytes_ = 0;
    Direction dir_ = UNKNOWN_DIRECTION;

  public:
    LteRlcUmRxEntity();
    ~LteRlcUmRxEntity() override;

    void enque(cPacket *pkt) override;
    void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer) override;
    bool isEmpty() const override;

    void handleMessage(cMessage *msg) override;

  protected:
    void initMode(LteMacBase *mac) override;

  private:
    void moveRxWindow(int pos);
    void reassemble(unsigned int index);
    void toPdcpLte(inet::Packet *rlcSdu);
    void clearBufferedSdu();
};

} //namespace

#endif
