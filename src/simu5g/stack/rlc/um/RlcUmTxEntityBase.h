//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_RLCUMTXENTITYBASE_H_
#define _SIMU5G_RLCUMTXENTITYBASE_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"

namespace simu5g {

class D2DModeController;

using namespace omnetpp;

/**
 * @brief Common shell of the RLC UM transmission entity.
 *
 * Holds the MAC plumbing, the D2D mode-switch machinery (holding buffer +
 * hold/resume) and the new-data notification shared by both RATs. The
 * wire-format specifics -- SDU buffering, PDU build, TX-buffer teardown and
 * the D2D drain -- are deferred to the concrete subclasses:
 *  - LteRlcUmTxEntity (TS 36.322): FI framing + concatenation, SN per PDU.
 *  - NrRlcUmTxEntity  (TS 38.322): SI + byte-offset (SO) segmentation, SN per SDU.
 *
 * The D2D hooks are inert when no D2DModeController is present (e.g. NR
 * standalone). Abstract base: not instantiated directly (no NED type, no
 * Define_Module); BearerManagement binds one of the concrete profiles.
 */
class RlcUmTxEntityBase : public RlcTxEntityBase
{
  protected:

    // Signals emitted by the concrete subclasses. Their static definitions live
    // in RlcUmTxEntityBase.cc (a single translation unit) so registerSignal()
    // ordering -- and therefore result recording -- stays stable across the split.
    static simsignal_t rlcPduCreatedSignal_;    // LTE: burst START/STOP tracking
    static simsignal_t wastedGrantedBytesSignal_;   // NR
    static simsignal_t requestedPduSizeSignal_;     // NR
    static simsignal_t sentPduSizeSignal_;          // NR
    static simsignal_t receivedPacketFromUpperLayerSignal_;   // NR
    static simsignal_t sentPacketToLowerLayerSignal_;         // NR

    // D2D mode switch controller (nullptr when D2D is not enabled)
    D2DModeController *d2dModeController_ = nullptr;

    // If true, the entity checks when the queue becomes empty (D2D).
    bool notifyEmptyBuffer_ = false;

    // If true, incoming SDUs go to the holding queue (D2D mode switch).
    bool holdingDownstreamInPackets_ = false;

    // The SDU holding buffer (D2D).
    inet::cPacketQueue sduHoldingQueue_;

    // Node id of the owner module
    MacNodeId ownerNodeId_;

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;

    // Stamp this flow's wire format onto its FlowControlInfo so the MAC/scheduler
    // can multiplex several SO PDUs into one grant (NR keeps one SDU/segment per PDU).
    void setFlowControlInfo(FlowControlInfo *info) override;

    // Common helpers shared by the concrete subclasses.
    virtual void dropBufferOverflow(inet::cPacket *pkt);
    virtual void sendPduToMac(inet::Packet *pkt);

    // --- mode-specific hooks, implemented by the concrete subclasses ---

    // Mode-specific part of initialize() (buffers, timers, mode parameters).
    virtual void initMode() {}

    // Buffer an incoming SDU; returns false on overflow (LTE bounded queue).
    virtual bool storeSdu(inet::Packet *pkt) = 0;

    // Wire format carried on FlowControlInfo (false = LTE FI/concat, true = NR SO).
    virtual bool usesSoFraming() const = 0;

    /*
     * Hook invoked for an SDU arriving from the upper layer, before it is
     * enqueued into the TX buffer. Returns true if the SDU was consumed by
     * the hook (e.g. held in the holding buffer during a D2D mode switch),
     * in which case the caller must neither enqueue it nor send a new-data
     * indication to the MAC layer.
     */
    virtual bool interceptSdu(inet::Packet *pkt);

    /*
     * Hook invoked after a PDU has been built, at the point where a TX buffer
     * that has just drained must be signaled (e.g. to notify the D2D mode
     * controller to resume holding-buffer packets for the newly selected mode).
     */
    virtual void onTxBufferEmptied();

    /*
     * Whether the TX buffer holds no further SDU data. The buffer itself is
     * mode-specific (LTE: SDU queue; NR: SO transmission buffer), so the
     * predicate behind onTxBufferEmptied() is deferred to the subclass.
     */
    virtual bool isTxBufferEmpty() const = 0;
    virtual unsigned int snFieldLength() const = 0;

  public:

    /**
     * handleSdu() is the main entry point for SDUs from the upper layer.
     * It adds the PDCP tracking tag, then enqueues/holds/drops the packet
     * and sends a new-data indication to MAC on successful enqueue.
     */
    virtual void handleSdu(inet::Packet *pkt);

    /**
     * handleMacSduRequest() handles a MAC SDU request packet: extracts the
     * requested size and calls rlcPduMake().
     */
    virtual void handleMacSduRequest(inet::Packet *pkt);

    // rlcPduMake() creates a PDU of the specified size and sends it to MAC.
    virtual void rlcPduMake(int pduSize) = 0;

    // clear the TX buffer
    virtual void clearQueue() = 0;

    // set holdingDownstreamInPackets_
    void startHoldingDownstreamInPackets() { holdingDownstreamInPackets_ = true; }

    // return true if the entity is holding incoming SDUs
    bool isHoldingDownstreamInPackets() { return holdingDownstreamInPackets_; }

    // store the packet in the holding buffer
    virtual void enqueHoldingPackets(inet::cPacket *pkt);

    // resume sending packets downstream
    virtual void resumeDownstreamInPackets() = 0;

    // return the value of notifyEmptyBuffer_
    bool isEmptyingBuffer() { return notifyEmptyBuffer_; }

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    virtual void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer = true) = 0;
};

} //namespace

#endif
