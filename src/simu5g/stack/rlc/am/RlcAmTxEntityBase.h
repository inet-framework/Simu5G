//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_RLCAMTXENTITYBASE_H_
#define _SIMU5G_RLCAMTXENTITYBASE_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief Common shell of the RLC AM transmission entity.
 *
 * Holds the signal set and the flow-control-info bookkeeping shared by both
 * RATs. The ARQ engine itself -- SDU buffering, PDU build, retransmission and
 * polling -- is deferred to the concrete subclasses:
 *  - LteRlcAmTxEntity (TS 36.322): fixed-unit fragmentation, whole-PDU ARQ
 *    retransmission with a per-PDU TMultiTimer, emits LteRlcAmPdu.
 *  - NrRlcAmTxEntity  (TS 38.322): byte-offset (SO) segmentation + re-segment
 *    on retransmission via a task-queue, polling, emits NrRlcAmDataPdu.
 * The MAC plumbing and the ACK_SN+NACK STATUS format are shared in spirit; the
 * two ARQ engines are different enough (whole-PDU vs SO re-segmentation) that
 * handleMessage() is not shared either. Abstract base: not instantiated
 * directly (no NED type, no Define_Module); BearerManagement binds one of the
 * concrete profiles.
 */
class RlcAmTxEntityBase : public RlcTxEntityBase
{
  protected:

    // Signals emitted by the concrete subclasses (both LTE and NR). Their
    // static definitions live in RlcAmTxEntityBase.cc (a single translation
    // unit) so registerSignal() ordering -- and therefore result recording --
    // stays stable across the split.
    static simsignal_t wastedGrantedBytesSignal_;
    static simsignal_t enqueuedSduSizeSignal_;
    static simsignal_t enqueuedSduRateSignal_;
    static simsignal_t requestedPduSizeSignal_;
    static simsignal_t txWindowOccupationSignal_;
    static simsignal_t txWindowFullSignal_;
    static simsignal_t retransmissionPduSignal_;
    static simsignal_t receivedPacketFromUpperLayerSignal_;
    static simsignal_t sentPacketToLowerLayerSignal_;

    // Copy of the flow control info of the SDU/fragment currently being
    // processed; stamped onto outgoing PDUs and new-data notifications by the
    // concrete subclasses.
    FlowControlInfo *lteInfo_ = nullptr;

    // Radio-link-failure state (TS 36.322 5.2.1 / TS 38.322 5.3.2): set once a unit
    // of this entity's ARQ (an SDU for NR, an AMD PDU for LTE) reaches
    // maxRtxThreshold_ retransmissions. Initialized by the concrete subclasses from
    // their maxRtxThreshold parameter.
    bool radioLinkFailureDetected_ = false;
    unsigned int maxRtxThreshold_ = 0;

    /**
     * Declare a radio link failure towards RRC: this entity's ARQ unit has reached
     * maxRtxThreshold_ retransmissions. BearerManagement tears down the bearer's
     * MAC/RLC/PDCP state at a safe point; repeat calls are absorbed there, so this
     * may be called from any path that hits the threshold.
     */
    virtual void declareRadioLinkFailure();

    ~RlcAmTxEntityBase() override { delete lteInfo_; }

  public:

    // Enqueue an upper-layer SDU into the transmission buffer
    virtual void enque(inet::Packet *sdu) = 0;

    // Send buffered PDUs until the requested grant size is reached
    virtual void sendPdus(int size) = 0;

    // Buffer a control (STATUS) PDU for transmission
    virtual void bufferControlPdu(cPacket *pkt) = 0;

    // Receive a control (STATUS) message from the AM receiver
    virtual void handleControlPacket(cPacket *pkt) = 0;

    // Pending data volume (bytes not yet sent to the MAC), used for buffer
    // status reporting
    virtual unsigned int getPendingDataVolume() const = 0;

    // The split threshold weighs the same pending data volume the buffer status reports.
    int64_t getBufferOccupancy() const override { return (int64_t)getPendingDataVolume(); }
};

} //namespace

#endif
