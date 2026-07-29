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

#ifndef _SIMU5G_RLCAMRXENTITYBASE_H_
#define _SIMU5G_RLCAMRXENTITYBASE_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"

namespace simu5g {

using namespace omnetpp;


/**
 * @class RlcAmRxEntityBase
 * @brief Common shell of the RLC AM receiving entity.
 *
 * Holds the signal set and the state used for control-PDU
 * routing to the local AM TX entity, and the STATUS-PDU flow-control-info /
 * throughput bookkeeping shared by both RATs. The reassembly / window /
 * timer logic is deferred to the concrete subclasses:
 *  - LteRlcAmRxEntity (TS 36.322): SN-contiguity defragmentation, PDU-SN
 *    window, consumes LteRlcAmPdu, emits an ACK_SN+NACK STATUS PDU.
 *  - NrRlcAmRxEntity  (TS 38.322): SO byte-coverage reassembly, SDU-SN
 *    window, t-Reassembly + t-StatusProhibit, consumes NrRlcAmDataPdu.
 * The two RX engines (and their control-PDU routing: LTE keys by dest, NR by
 * source + creates on demand) are different enough that handleMessage() is
 * not shared either. Abstract base: not instantiated directly (no NED type,
 * no Define_Module); BearerManagement binds one of the concrete profiles.
 */
class RlcAmRxEntityBase : public RlcRxEntityBase
{
  protected:

    // Signals emitted by the concrete subclasses. Declared/registered in one
    // translation unit (RlcAmRxEntityBase.cc), in their historical order, so
    // registerSignal() ordering -- and therefore result recording -- is
    // unaffected by the split.
    static simsignal_t rlcPacketLossSignal_[2];
    static simsignal_t rlcPduPacketLossSignal_[2];
    static simsignal_t rlcDelaySignal_[2];
    static simsignal_t rlcThroughputSignal_[2];
    static simsignal_t rlcPduDelaySignal_[2];
    static simsignal_t rlcPduThroughputSignal_[2];
    static simsignal_t receivedPacketFromLowerLayerSignal_;
    static simsignal_t sentPacketToUpperLayerSignal_;
    static simsignal_t rxWindowOccupationSignal_;


    // The reversed flow info used to stamp outgoing STATUS PDUs, built from the
    // first received data PDU's flow info.
    FlowControlInfo *ackFlowControlInfo_ = nullptr;
    simtime_t lastSentAck_ = 0;

    // Per-bearer throughput accounting, common to both passUp implementations.
    unsigned int totalRcvdBytes_ = 0;
    unsigned int totalPduRcvdBytes_ = 0;

    /**
     * Emit this bearer's delay and throughput statistics for a received PDU
     * (perPdu = true, before reassembly) or a delivered SDU (perPdu = false), the
     * latter together with the always-zero AM packet-loss sample.
     */
    void emitRxStatistics(bool perPdu, double throughput, omnetpp::simtime_t delay);

    void initialize(int stage) override;

    // Mode-specific INITSTAGE_LOCAL init (buffers, timers, window).
    virtual void initMode() = 0;

  public:

    // Enqueues a lower-layer PDU into the entity for reassembly
    virtual void enque(inet::Packet *pdu) = 0;

    ~RlcAmRxEntityBase() override { delete ackFlowControlInfo_; }
};

} //namespace

#endif
