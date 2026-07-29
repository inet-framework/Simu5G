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

#ifndef _SIMU5G_NRRLCAMRXENTITY_H_
#define _SIMU5G_NRRLCAMRXENTITY_H_

#include <set>
#include <string>

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/rlc/am/RlcAmRxEntityBase.h"
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowReceptionBuffer.h"

namespace simu5g {

using namespace omnetpp;

class RlcMux;

/**
 * @brief NR (TS 38.322) RLC AM receiving entity.
 *
 * SO byte-coverage reassembly with a per-SDU SN window, t-Reassembly +
 * t-StatusProhibit timers, consumes NrRlcAmDataPdu.
 */
class NrRlcAmRxEntity : public RlcAmRxEntityBase
{
    std::string nameEntity_;
    RlcSduSlidingWindowReceptionBuffer *rxBuffer_ = nullptr;
    std::set<unsigned int> passedUpSdus_;
    omnetpp::cMessage *tReassemblyTimer_ = nullptr;
    omnetpp::simtime_t tReassembly_;
    omnetpp::cMessage *tStatusProhibitTimer_ = nullptr;
    omnetpp::simtime_t tStatusProhibit_;
    unsigned int rxNextStatusTrigger_ = 0;
    unsigned int amWindowSize_ = 0;
    bool statusReportPending_ = false;

  public:
    ~NrRlcAmRxEntity() override;

    void enque(inet::Packet *pkt) override;
    void handleMessage(cMessage *msg) override;

  protected:

    void initMode() override;

  private:

    void passUpNr(int seqNum);
    void sendStatusReportNr();
    void routeControlToTxEntityNr(inet::Packet *pkt);
    void bufferControlViaTxEntityNr(inet::Packet *pkt);
};

} //namespace

#endif
