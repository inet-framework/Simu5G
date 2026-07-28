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

#ifndef _SIMU5G_NRRLCUMRXENTITY_H_
#define _SIMU5G_NRRLCUMRXENTITY_H_

#include <set>
#include <deque>

#include "simu5g/stack/rlc/um/RlcUmRxEntityBase.h"
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"
#include "simu5g/stack/rlc/um/RlcUmReceptionBuffer.h"

namespace simu5g {

using namespace omnetpp;

class LteMacBase;

/**
 * @class NrRlcUmRxEntity
 * @brief NR (TS 38.322) RLC UM receiving entity.
 *
 * SI + byte-offset (SO) reassembly with a per-SDU SN window and a t-Reassembly
 * timer. A complete SDU (FI=00) is delivered immediately; segments are buffered
 * for byte-coverage reassembly.
 */
class NrRlcUmRxEntity : public RlcUmRxEntityBase
{
    // The SO reassembly buffer.
    RlcUmReceptionBuffer *sduBuffer = nullptr;
    int UM_Window_Size = 2048;
    cMessage *t_ReassemblyTimer = nullptr;
    simtime_t t_Reassembly;
    unsigned long totalRcvdBytesNr_ = 0;

    // Duplicate detection for complete SDUs (which carry no RLC SN); snoMainPacket
    // values delivered within the last t_Reassembly window, for discarding HARQ-
    // re-delivered duplicate TBs. See enque().
    std::set<unsigned int> recentCompleteSduSet_;
    std::deque<std::pair<simtime_t, unsigned int>> recentCompleteSduQueue_;

    unsigned int totalPduRcvdBytes_ = 0;

    // Emit this bearer's delay and throughput statistics for a received PDU or a
    // delivered SDU, in the D2D or the infrastructure variant.
    void emitRxStatistics(bool perPdu, double throughput, simtime_t delay);

  public:
    ~NrRlcUmRxEntity() override;

    void enque(cPacket *pkt) override;
    void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer) override;
    bool isEmpty() const override;

    void handleMessage(cMessage *msg) override;

  protected:
    void initMode(LteMacBase *mac) override;

  private:
    void handlePDUInReceivedBuffer(inet::Ptr<NrRlcUmDataPdu> pdu, unsigned int tsn);
    void toPdcpNr(inet::Packet *rlcSdu);
};

} //namespace

#endif
