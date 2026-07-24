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

#include <inet/common/ProtocolTag_m.h>

#include "simu5g/stack/rlc/um/LteRlcUmTxEntity.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowSignals.h"
#include "simu5g/stack/rrc/D2DModeController.h"

namespace simu5g {

Define_Module(LteRlcUmTxEntity);

using namespace inet;

void LteRlcUmTxEntity::initMode()
{
    queueSize_ = par("queueSize");
    burstStatus_ = INACTIVE;
}

bool LteRlcUmTxEntity::storeSdu(inet::Packet *pkt)
{
    return enque(pkt);
}

bool LteRlcUmTxEntity::enque(cPacket *pkt)
{
    EV << NOW << " LteRlcUmTxEntity::enque - buffering new SDU  " << endl;
    if (queueSize_ == 0 || queueLength_ + pkt->getByteLength() < queueSize_) {
        // Buffer the SDU in the TX buffer
        sduQueue_.insert(pkt);
        queueLength_ += pkt->getByteLength();
        return true;
    }
    else {
        // Buffer is full - cannot enqueue packet
        return false;
    }
}

void LteRlcUmTxEntity::rlcPduMake(int pduLength)
{
    EV << NOW << " LteRlcUmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;

    // create the RLC PDU
    auto pkt = new inet::Packet("lteRlcFragment");
    auto rlcPdu = inet::makeShared<LteRlcUmDataPdu>();

    // the request from MAC takes into account also the size of the RLC header
    pduLength -= RLC_HEADER_UM;

    int len = 0;

    bool startFrag = firstIsFragment_;
    bool endFrag = false;

    while (!sduQueue_.isEmpty() && pduLength > 0) {
        // detach data from the SDU buffer
        auto pkt = check_and_cast<inet::Packet *>(sduQueue_.front());
        auto pdcpTag = pkt->getTag<PdcpTrackingTag>();
        unsigned int sduSequenceNumber = pdcpTag->getPdcpSequenceNumber();
        int sduLength = pdcpTag->getOriginalPacketLength();

        if (fragmentInfo != nullptr) {
            if (fragmentInfo->pkt != pkt)
                throw cRuntimeError("Packets are different");
            sduLength = fragmentInfo->size;
        }

        EV << NOW << " LteRlcUmTxEntity::rlcPduMake - Next data chunk from the queue, sduSno[" << sduSequenceNumber
           << "], length[" << sduLength << "]" << endl;

        if (pduLength >= sduLength) {
            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - Add " << sduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

            // add the whole SDU
            if (fragmentInfo) {
                delete fragmentInfo;
                fragmentInfo = nullptr;
            }
            pduLength -= sduLength;
            len += sduLength;

            pkt = check_and_cast<inet::Packet *>(sduQueue_.pop());
            queueLength_ -= pkt->getByteLength();

            rlcPdu->pushSdu(pkt, sduLength);
            pkt = nullptr;

            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - Pop data chunk from the queue, sduSno[" << sduSequenceNumber << "]" << endl;

            // now, the first SDU in the buffer is not a fragment
            firstIsFragment_ = false;

            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;
        }
        else {
            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - Add " << pduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

            // add partial SDU
            len += pduLength;

            auto rlcSduDup = pkt->dup();
            if (fragmentInfo != nullptr) {
                fragmentInfo->size -= pduLength;
                if (fragmentInfo->size < 0)
                    throw cRuntimeError("Fragmentation error");
            }
            else {
                fragmentInfo = new FragmentInfo;
                fragmentInfo->pkt = pkt;
                fragmentInfo->size = sduLength - pduLength;
            }
            rlcPdu->pushSdu(rlcSduDup, pduLength);

            endFrag = true;

            // update SDU in the buffer
            int newLength = sduLength - pduLength;

            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - Data chunk in the queue is now " << newLength << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            pduLength = 0;

            // now, the first SDU in the buffer is a fragment
            firstIsFragment_ = true;

            EV << NOW << " LteRlcUmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;
        }
    }

    if (len == 0) {
        // send an empty (1-bit) message to notify the MAC that there is not enough space
        EV << NOW << " LteRlcUmTxEntity::rlcPduMake - cannot send PDU with data, pdulength requested by MAC (" << pduLength << "B) is too small." << std::endl;
        pkt->setName("lteRlcFragment (empty)");
        rlcPdu->setChunkLength(inet::b(1)); // send only a bit, minimum size
    }
    else {
        // compute FI (3GPP TS 36.322)
        FramingInfo fi;
        fi.firstIsFragment = startFrag;   // 10
        fi.lastIsFragment = endFrag;      // 01

        rlcPdu->setFramingInfo(fi);
        rlcPdu->setPduSequenceNumber(sno_++);
        rlcPdu->setChunkLength(inet::B(RLC_HEADER_UM + len));
    }

    *pkt->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;

    /*
     * @author Alessandro Noferi
     * Notify the packetFlowObserver about the new RLC PDU only in UL or DL cases
     */
    if (flowControlInfo_->getDirection() == DL || flowControlInfo_->getDirection() == UL) {
        if (len != 0 && hasListeners(rlcPduCreatedSignal_)) {
            DrbKey drbKey = ctrlInfoToTxDrbKey(flowControlInfo_);

            /*
             * Burst management. If the buffer is empty, an ACTIVE burst is now
             * finished (STOP). If not empty, START a burst when INACTIVE.
             */
            if (sduQueue_.isEmpty()) {
                if (burstStatus_ == ACTIVE) {
                    EV << NOW << " LteRlcUmTxEntity::burstStatus - ACTIVE -> INACTIVE" << endl;
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), STOP);
                    emit(rlcPduCreatedSignal_, &info);
                    burstStatus_ = INACTIVE;
                }
                else {
                    EV << NOW << " LteRlcUmTxEntity::burstStatus - " << burstStatus_ << endl;
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), burstStatus_);
                    emit(rlcPduCreatedSignal_, &info);
                }
            }
            else {
                if (burstStatus_ == INACTIVE) {
                    burstStatus_ = ACTIVE;
                    EV << NOW << " LteRlcUmTxEntity::burstStatus - INACTIVE -> ACTIVE" << endl;
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), START);
                    emit(rlcPduCreatedSignal_, &info);
                }
                else {
                    EV << NOW << " LteRlcUmTxEntity::burstStatus - burstStatus: " << burstStatus_ << endl;
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), burstStatus_);
                    emit(rlcPduCreatedSignal_, &info);
                }
            }
        }
    }

    // send to MAC layer
    pkt->insertAtFront(rlcPdu);
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    EV << NOW << " LteRlcUmTxEntity::rlcPduMake - send PDU " << rlcPdu->getPduSequenceNumber() << " with size " << pkt->getByteLength() << " bytes to lower layer" << endl;
    send(pkt, "out");

    // if incoming connection was halted
    if (notifyEmptyBuffer_ && sduQueue_.isEmpty()) {
        notifyEmptyBuffer_ = false;
        // tell the D2D mode controller to resume packets for the new mode
        if (d2dModeController_)
            d2dModeController_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void LteRlcUmTxEntity::removeDataFromQueue()
{
    EV << NOW << " LteRlcUmTxEntity::removeDataFromQueue - removed SDU " << endl;
    cPacket *pkt = sduQueue_.back();
    cPacket *retPkt = sduQueue_.remove(pkt);
    queueLength_ -= retPkt->getByteLength();
    ASSERT(queueLength_ >= 0);
    delete retPkt;
}

void LteRlcUmTxEntity::clearQueue()
{
    // empty buffer
    while (!sduQueue_.isEmpty())
        delete sduQueue_.pop();

    if (fragmentInfo) {
        delete fragmentInfo;
        fragmentInfo = nullptr;
    }

    queueLength_ = 0;

    // reset variables except for sequence number
    firstIsFragment_ = false;
}

void LteRlcUmTxEntity::resumeDownstreamInPackets()
{
    EV << NOW << " LteRlcUmTxEntity::resumeDownstreamInPackets - resume buffering incoming downstream packets of the RLC entity associated with the new mode" << endl;

    holdingDownstreamInPackets_ = false;

    // move all SDUs in the holding buffer to the TX buffer
    while (!sduHoldingQueue_.isEmpty()) {
        auto pktRlc = check_and_cast<inet::Packet *>(sduHoldingQueue_.front());
        sduHoldingQueue_.pop();

        // store the SDU in the TX buffer
        if (enque(pktRlc)) {
            auto pktRlcdup = pktRlc->dup();
            pktRlcdup->addTag<LteRlcNewDataTag>();
            send(pktRlcdup, "out");
        }
        else {
            EV << "LteRlcUmTxEntity::resumeDownstreamInPackets - cannot buffer SDU (queue is full), dropping" << std::endl;
            dropBufferOverflow(pktRlc);
        }
    }
}

void LteRlcUmTxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {
            EV << NOW << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - clear TX buffer of the RLC entity associated with the old mode" << endl;
            clearQueue();
        }
        else {
            if (!sduQueue_.isEmpty()) {
                EV << NOW << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - check when the TX buffer of the old mode becomes empty - queue length[" << sduQueue_.getLength() << "]" << endl;
                notifyEmptyBuffer_ = true;
            }
            else {
                EV << NOW << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - TX buffer of the old mode is already empty" << endl;
            }
        }
    }
    else {
        EV << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - reset numbering of the RLC TX entity corresponding to the new mode" << endl;
        sno_ = 0;

        if (!clearBuffer) {
            if (d2dModeController_ && d2dModeController_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId())) {
                EV << NOW << " LteRlcUmTxEntity::rlcHandleD2DModeSwitch - halt incoming downstream connections of the new mode" << endl;
                startHoldingDownstreamInPackets();
            }
        }
    }
}

} //namespace
