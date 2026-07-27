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

#ifndef _LTE_UMRXENTITY_H_
#define _LTE_UMRXENTITY_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/common/timer/TTimer.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"
#include "simu5g/stack/rlc/um/RlcUmReceptionBuffer.h"

namespace simu5g {

using namespace omnetpp;

class LteMacBase;
class RlcMux;
class LteRlcUmDataPdu;

/**
 * @class UmRxEntity
 * @brief Generic RLC UM receiving entity, parametrized for LTE or NR.
 *
 * One mechanism, two wire-format parametrizations selected by the soFraming flag:
 *  - LTE (soFraming=false, TS 36.322): FI-walk reassembly across concatenated PDUs,
 *    PDU-SN reordering window, t-Reordering.
 *  - NR  (soFraming=true,  TS 38.322): SI + byte-offset (SO) reassembly, SDU-SN
 *    reassembly window, t-Reassembly.
 *
 * The MAC-mux plumbing, the D2D mode-switch hooks and the UL burst-throughput
 * accounting are shared; only the buffering/reassembly/timer logic differs per mode.
 */
class UmRxEntity : public RlcRxEntityBase
{
  protected:

    // --- wire-format selector (profile-driven; see UmTxEntity) ---
    bool soFraming_ = false;

    // --- shared ---
    // Node id of the owner module
    MacNodeId ownerNodeId_;
    // The mux feeding this entity (for UL burst-throughput reporting)
    RlcMux *rlcMux_ = nullptr;
    // After a D2D mode switch, the first PDU on the new-mode entity is forced in-sequence
    bool resetFlag_ = false;

    // UL data-burst accounting (TS 136.314), eNB only
    enum BurstCheck { ENQUE, REORDERING };
    bool isBurst_ = false;
    unsigned int totalBits_ = 0;
    unsigned int ttiBits_ = 0;
    simtime_t t1_ = 0;
    simtime_t t2_ = 0;

    static simsignal_t rlcCellThroughputSignal_[2];

    // --- LTE (FI/concatenation) state ---
    inet::ModuleRefByPar<Binder> binder_;
    opp_component_ptr<cModule> nodeB_;
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
    static unsigned int totalCellPduRcvdBytes_;
    static unsigned int totalCellRcvdBytes_;
    unsigned int totalPduRcvdBytes_ = 0;
    unsigned int totalRcvdBytes_ = 0;
    Direction dir_ = UNKNOWN_DIRECTION;
    static simsignal_t rlcDelaySignal_[2];
    static simsignal_t rlcPduDelaySignal_[2];
    static simsignal_t rlcThroughputSignal_[2];
    static simsignal_t rlcPduThroughputSignal_[2];
    static simsignal_t rlcDelayD2DSignal_;
    static simsignal_t rlcPduDelayD2DSignal_;
    static simsignal_t rlcThroughputD2DSignal_;
    static simsignal_t rlcPduThroughputD2DSignal_;

    // --- NR (SO byte-offset) state ---
    RlcUmReceptionBuffer *sduBuffer = nullptr;
    int UM_Window_Size = 2048;
    cMessage *t_ReassemblyTimer = nullptr;
    simtime_t t_Reassembly;
    unsigned long totalRcvdBytesNr_ = 0;
    static simsignal_t receivedPacketFromLowerLayerSignal_;
    static simsignal_t sentPacketToUpperLayerSignal_;

  public:
    UmRxEntity();
    ~UmRxEntity() override;

    // Enqueues a lower-layer PDU into the entity for reassembly
    void enque(cPacket *pkt);

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer = true);

    // returns if the entity contains RLC pdus
    bool isEmpty() const;

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;

  private:

    // UL throughput burst accounting (shared accumulator; per-mode buffer-empty check)
    void handleBurst(BurstCheck event);

    // --- LTE (FI/concatenation) implementation ---
    void enqueLte(cPacket *pktPdu);
    void moveRxWindow(int pos);
    void reassemble(unsigned int index);
    void toPdcpLte(inet::Packet *rlcSdu);
    void clearBufferedSdu();
    void rlcHandleD2DModeSwitchLte(bool oldConnection, bool oldMode, bool clearBuffer);

    // --- NR (SO byte-offset) implementation ---
    void enqueNr(inet::Packet *pkt);
    void handlePDUInReceivedBuffer(inet::Ptr<NrRlcUmDataPdu> pdu, unsigned int tsn);
    void toPdcpNr(inet::Packet *rlcSdu);
    void rlcHandleD2DModeSwitchNr(bool oldConnection, bool oldMode, bool clearBuffer);
};

} //namespace

#endif
