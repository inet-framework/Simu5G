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

#ifndef _LTE_UMTXENTITY_H_
#define _LTE_UMTXENTITY_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"
#include "simu5g/stack/rlc/um/RlcUmTransmitterBuffer.h"
#include "simu5g/mec/utils/MecCommon.h"

namespace simu5g {

class D2DModeController;

using namespace omnetpp;

/**
 * @class UmTxEntity
 * @brief Generic RLC UM transmission entity, parametrized for LTE or NR.
 *
 * One mechanism, two wire-format parametrizations selected by the isNR flag:
 *  - LTE (isNR=false, TS 36.322): FI framing + concatenation of multiple SDUs
 *    per PDU, sequence number per PDU, emits LteRlcUmDataPdu.
 *  - NR  (isNR=true,  TS 38.322): SI + byte-offset (SO) segmentation, one SDU
 *    segment per PDU, sequence number per SDU, emits NrRlcUmDataPdu.
 *
 * The MAC plumbing, the D2D mode-switch machinery (holding buffer + hold/resume)
 * and the new-data notification are shared; only the buffering and PDU build
 * differ per mode. The D2D hooks are inert when no D2DModeController is present
 * (e.g. NR standalone). Select the parametrization via the NED profile bound to
 * BearerManagement.rlcUmTxEntityModuleType.
 */
class UmTxEntity : public RlcTxEntityBase
{
    // --- wire-format selector (profile-driven; NOT the NIC-leg isNR) ---
    // false = LTE FI/concatenation (TS 36.322); true = NR SI/SO byte-offset (TS 38.322)
    bool soFraming_ = false;

    // --- LTE (FI/concatenation) state ---
    static simsignal_t rlcPduCreatedSignal_;
    struct FragmentInfo {
        inet::Packet *pkt = nullptr;
        int size = 0;
    };
    FragmentInfo *fragmentInfo = nullptr;

    // --- NR (SO segmentation) state ---
    RlcUmTransmitterBuffer *sduBuffer = nullptr;
    unsigned int sn_FieldLength = 12;
    static simsignal_t wastedGrantedBytes;
    static simsignal_t requestedPDUSizeSignal;
    static simsignal_t sentPDUSizeSignal;

  public:

    ~UmTxEntity() override
    {
        delete fragmentInfo;
        if (sduBuffer) {
            sduBuffer->clearBuffer();
            delete sduBuffer;
        }
    }

    /**
     * handleSdu() is the main entry point for SDUs from the upper layer.
     * It adds the PDCP tracking tag, then enqueues/holds/drops the packet
     * and sends a new-data indication to MAC on successful enqueue.
     */
    void handleSdu(inet::Packet *pkt);

    /*
     * Enqueues an upper layer packet into the SDU buffer (LTE mode)
     * @return TRUE if the packet was enqueued in the SDU buffer
     */
    bool enque(cPacket *pkt);

    /**
     * handleMacSduRequest() handles a MAC SDU request packet.
     * Extracts the requested size and calls rlcPduMake().
     */
    void handleMacSduRequest(inet::Packet *pkt);

    /**
     * rlcPduMake() creates a PDU of the specified size and sends it to MAC.
     */
    void rlcPduMake(int pduSize);

    // force the sequence number (LTE mode)
    void setNextSequenceNumber(unsigned int nextSno) { sno_ = nextSno; }

    // drop fragments if the queue is full (LTE mode)
    void dropBufferOverflow(cPacket *pkt);

    // remove the last SDU from the queue (LTE mode)
    void removeDataFromQueue();

    // clear the TX buffer
    void clearQueue();

    // set holdingDownstreamInPackets_
    void startHoldingDownstreamInPackets() { holdingDownstreamInPackets_ = true; }

    // return true if the entity is holding incoming SDUs
    bool isHoldingDownstreamInPackets();

    // store the packet in the holding buffer
    void enqueHoldingPackets(inet::cPacket *pkt);

    // resume sending packets downstream
    void resumeDownstreamInPackets();

    // return the value of notifyEmptyBuffer_
    bool isEmptyingBuffer() { return notifyEmptyBuffer_; }

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer = true);

  protected:

    // D2D mode switch controller (nullptr when D2D is not enabled)
    D2DModeController *d2dModeController_ = nullptr;

    /*
     * @author Alessandro Noferi
     * reference to packetFlowObserver in order to count discarded packets and
     * packet delay (LTE mode burst tracking; null-safe via hasListeners()).
     */
    RlcBurstStatus burstStatus_;

    // The SDU enqueue buffer (LTE mode).
    inet::cPacketQueue sduQueue_;

    // Whether the first item in the LTE queue is a fragment or a whole SDU.
    bool firstIsFragment_ = false;

    // If true, the entity checks when the queue becomes empty (D2D).
    bool notifyEmptyBuffer_ = false;

    // If true, incoming SDUs go to the holding queue (D2D mode switch).
    bool holdingDownstreamInPackets_ = false;

    // The SDU holding buffer (D2D).
    inet::cPacketQueue sduHoldingQueue_;

    // The maximum available queue size, in bytes (LTE mode).
    unsigned int queueSize_;

    // The currently stored amount of data in the LTE SDU queue, in bytes.
    unsigned int queueLength_ = 0;

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;

  private:

    // Node id of the owner module
    MacNodeId ownerNodeId_;

    /// Next PDU sequence number to be assigned (LTE mode)
    unsigned int sno_ = 0;

    // --- mode-specific implementations (behaviour-preserving) ---
    void rlcPduMakeLte(int pduSize);
    void rlcPduMakeNr(int pduSize);
    void clearQueueLte();
    void clearQueueNr();
    void resumeDownstreamInPacketsLte();
    void resumeDownstreamInPacketsNr();
    void rlcHandleD2DModeSwitchLte(bool oldConnection, bool clearBuffer);
    void rlcHandleD2DModeSwitchNr(bool oldConnection, bool clearBuffer);

    // NR helpers
    void sendPduToMac(inet::Packet *pkt);
    void notifyControllerIfEmptied();
};

} //namespace

#endif
