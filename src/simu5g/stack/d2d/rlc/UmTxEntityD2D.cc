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

#include "simu5g/stack/d2d/rlc/UmTxEntityD2D.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/d2d/rrc/D2DModeController.h"

namespace simu5g {

Define_Module(UmTxEntityD2D);

// NOTE: no static registerSignal() calls in this translation unit -- all
// signals used by this module are registered in UmTxEntity.cc (global
// signal-ID order stability, sz fingerprint).

using namespace inet;

void UmTxEntityD2D::initialize(int stage)
{
    UmTxEntity::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        auto *rrc = getParentModule()->getSubmodule("rrc");
        d2dModeController_ = dynamic_cast<D2DModeController *>(rrc ? rrc->getSubmodule("d2dModeController") : nullptr);
    }
}

void UmTxEntityD2D::setFlowControlInfo(FlowControlInfo *info)
{
    UmTxEntity::setFlowControlInfo(info);

    // D2D peer tracking: register with the D2D mode controller, so that mode
    // switches can coordinate the holding/draining of this entity's buffers
    // (moved here from BearerManagement)
    if (d2dModeController_)
        d2dModeController_->registerD2DPeerTxEntity(MacNodeId(flowControlInfo_->getD2dRxPeerId()), this);
}

bool UmTxEntityD2D::interceptSdu(inet::Packet *pkt)
{
    if (holdingDownstreamInPackets_) {
        // do not store in the TX buffer and do not signal the MAC layer
        EV << "UmTxEntityD2D::interceptSdu - Enqueue packet into the Holding Buffer\n";
        enqueHoldingPackets(pkt);
        return true;
    }
    return UmTxEntity::interceptSdu(pkt);
}

void UmTxEntityD2D::onTxBufferEmptied()
{
    // if incoming connection was halted
    if (notifyEmptyBuffer_ && sduQueue_.isEmpty()) {
        notifyEmptyBuffer_ = false;

        // tell the D2D mode controller to resume packets for the new mode
        if (d2dModeController_)
            d2dModeController_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

bool UmTxEntityD2D::isHoldingDownstreamInPackets()
{
    return holdingDownstreamInPackets_;
}

void UmTxEntityD2D::enqueHoldingPackets(cPacket *pkt)
{
    EV << NOW << " UmTxEntityD2D::enqueHoldingPackets - storing new SDU into the holding buffer " << endl;
    sduHoldingQueue_.insert(pkt);
}

void UmTxEntityD2D::resumeDownstreamInPackets()
{
    EV << NOW << " UmTxEntityD2D::resumeDownstreamInPackets - resume buffering incoming downstream packets of the RLC entity associated with the new mode" << endl;

    holdingDownstreamInPackets_ = false;

    // move all SDUs in the holding buffer to the TX buffer
    while (!sduHoldingQueue_.isEmpty()) {
        auto pktRlc = check_and_cast<inet::Packet *>(sduHoldingQueue_.front());

        sduHoldingQueue_.pop();

        // store the SDU in the TX buffer
        if (enque(pktRlc)) {
            // create a message to notify the MAC layer that the queue contains new data
            // make a copy of the RLC SDU
            auto pktRlcdup = pktRlc->dup();
            // add tag to indicate new data availability to MAC
            pktRlcdup->addTag<LteRlcNewDataTag>();
            // send the new data indication to the MAC
            send(pktRlcdup, "out");
        }
        else {
            // Queue is full - drop SDU
            EV << "UmTxEntityD2D::resumeDownstreamInPackets - cannot buffer SDU (queue is full), dropping" << std::endl;
            dropBufferOverflow(pktRlc);
        }
    }
}

void UmTxEntityD2D::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " UmTxEntityD2D::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {
            EV << NOW << " UmTxEntityD2D::rlcHandleD2DModeSwitch - clear TX buffer of the RLC entity associated with the old mode" << endl;
            clearQueue();
        }
        else {
            if (!sduQueue_.isEmpty()) {
                EV << NOW << " UmTxEntityD2D::rlcHandleD2DModeSwitch - check when the TX buffer of the RLC entity associated with the old mode becomes empty - queue length[" << sduQueue_.getLength() << "]" << endl;
                notifyEmptyBuffer_ = true;
            }
            else {
                EV << NOW << " UmTxEntityD2D::rlcHandleD2DModeSwitch - TX buffer of the RLC entity associated with the old mode is already empty" << endl;
            }
        }
    }
    else {
        EV << " UmTxEntityD2D::rlcHandleD2DModeSwitch - reset numbering of the RLC TX entity corresponding to the new mode" << endl;
        setNextSequenceNumber(0);

        if (!clearBuffer) {
            if (d2dModeController_ && d2dModeController_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId())) {
                // stop incoming connections, until
                EV << NOW << " UmTxEntityD2D::rlcHandleD2DModeSwitch - halt incoming downstream connections of the RLC entity associated with the new mode" << endl;
                startHoldingDownstreamInPackets();
            }
        }
    }
}

} //namespace
