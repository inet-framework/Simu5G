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

#ifndef _LTE_UMTXENTITY_D2D_H_
#define _LTE_UMTXENTITY_D2D_H_

#include "simu5g/stack/rlc/um/UmTxEntity.h"

namespace simu5g {

class D2DModeController;

using namespace omnetpp;

/**
 * @class UmTxEntityD2D
 * @brief D2D-aware transmission entity for UM.
 *
 * Extends ~UmTxEntity with the machinery needed by D2D mode switching:
 * while the RLC entity associated with the old mode is draining its TX
 * buffer, incoming SDUs are parked in a holding queue and released to
 * the TX buffer once the drain completes (coordinated through the
 * D2DModeController). Created by BearerManagement on the D2D-capable
 * NICs.
 */
class UmTxEntityD2D : public UmTxEntity
{
  public:
    // registers with the D2D mode controller once the peer id is known
    void setFlowControlInfo(FlowControlInfo *info) override;

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer = true);

    // set holdingDownstreamInPackets_
    void startHoldingDownstreamInPackets() { holdingDownstreamInPackets_ = true; }

    // return true if the entity is not buffering in the TX queue
    bool isHoldingDownstreamInPackets();

    // store the packet in the holding buffer
    void enqueHoldingPackets(inet::cPacket *pkt);

    // resume sending packets downstream
    void resumeDownstreamInPackets();

    // return the value of notifyEmptyBuffer_
    bool isEmptyingBuffer() { return notifyEmptyBuffer_; }

  protected:

    // D2D mode switch controller (nullptr when the NIC has none)
    D2DModeController *d2dModeController_ = nullptr;

    /*
     * If true, the entity checks when the queue becomes empty
     */
    bool notifyEmptyBuffer_ = false;

    /*
     * If true, the entity temporarily stores incoming SDUs in the holding queue (useful at D2D mode switching)
     */
    bool holdingDownstreamInPackets_ = false;

    /*
     * The SDU holding buffer.
     */
    inet::cPacketQueue sduHoldingQueue_;

    void initialize(int stage) override;

    // holds the SDU in the holding buffer during a D2D mode switch
    bool interceptSdu(inet::Packet *pkt) override;

    // notifies the D2D mode controller when the TX buffer of the old mode has drained
    void onTxBufferEmptied() override;
};

} //namespace

#endif
