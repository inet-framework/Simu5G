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

#ifndef _SIMU5G_RLCAMRXENTITY_H_
#define _SIMU5G_RLCAMRXENTITY_H_

#include <set>

#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/timer/TTimer.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowReceptionBuffer.h"

namespace simu5g {

using namespace omnetpp;

class BearerManagement;
class RlcMux;

/*
 * Generic RLC AM Reception Entity, parametrized for LTE or NR via soFraming:
 *  - LTE (soFraming=false, TS 36.322): SN-contiguity defragmentation, PDU-SN window,
 *    consumes LteRlcAmPdu, emits an ACK_SN+NACK STATUS PDU.
 *  - NR  (soFraming=true,  TS 38.322): SO byte-coverage reassembly, SDU-SN window,
 *    t-Reassembly + t-StatusProhibit, consumes NrRlcAmDataPdu.
 * The control-PDU routing to the local AM TX entity is shared in spirit (mode-specific
 * DrbKey derivation: LTE keys by dest, NR by source + creates on demand).
 */
class RlcAmRxEntity : public RlcRxEntityBase
{
  protected:

    // --- wire-format selector (profile-driven; see RlcUmTxEntity) ---
    bool soFraming_ = false;

    // --- shared ---
    BearerManagement *bearerManagement_ = nullptr;
    FlowControlInfo *ackFlowControlInfo_ = nullptr;
    simtime_t lastSentAck_;
    static unsigned int totalCellRcvdBytes_;
    unsigned int totalRcvdBytes_ = 0;
    static simsignal_t rlcCellThroughputSignal_[2];

    // --- LTE (fragment / SN-contiguity) state ---
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
    static simsignal_t rlcCellPacketLossSignal_[2];
    static simsignal_t rlcPacketLossSignal_[2];
    static simsignal_t rlcPduPacketLossSignal_[2];
    static simsignal_t rlcDelaySignal_[2];
    static simsignal_t rlcPduDelaySignal_[2];
    static simsignal_t rlcThroughputSignal_[2];
    static simsignal_t rlcPduThroughputSignal_[2];

    // --- NR (SO byte-coverage) state ---
    RlcMux *rlcMux_ = nullptr;
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
    static omnetpp::simsignal_t receivedPacketFromLowerLayerSignal_;
    static omnetpp::simsignal_t sentPacketToUpperLayerSignal_;
    static omnetpp::simsignal_t rxWindowOccupationSignal_;

  public:
    RlcAmRxEntity();
    ~RlcAmRxEntity() override;

    void enque(inet::Packet *pdu);
    void handleMessage(cMessage *msg) override;
    void initialize(int stage) override;

  protected:

    // --- LTE implementation ---
    void enqueLte(inet::Packet *pdu);
    void passUpLte(const int index);
    void checkCompleteSdu(const int index);
    void sendStatusReportLte();
    int computeWindowShift() const;
    void moveRxWindow(const int seqNum);
    void discard(const int sn);
    inet::Packet *defragmentFrames(std::deque<inet::Packet *>& fragmentFrames);
    void routeControlToTxEntityLte(inet::Packet *pkt);
    void bufferControlViaTxEntityLte(inet::Packet *pkt);

    // --- NR implementation ---
    void enqueNr(inet::Packet *pkt);
    void passUpNr(int seqNum);
    void sendStatusReportNr();
    void routeControlToTxEntityNr(inet::Packet *pkt);
    void bufferControlViaTxEntityNr(inet::Packet *pkt);
};

} //namespace

#endif
