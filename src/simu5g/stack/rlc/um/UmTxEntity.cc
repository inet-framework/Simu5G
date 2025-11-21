//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/rlc/um/UmTxEntity.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

#include "simu5g/stack/packetFlowObserver/PacketFlowObserverUe.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowObserverEnb.h"

#include <inet/common/packet/chunk/SliceChunk.h>

namespace simu5g {

Define_Module(UmTxEntity);

using namespace inet;

/*
 * Main functions
 */

void UmTxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        LteMacBase *mac = inet::getConnectedModule<LteMacBase>(getParentModule()->gate("RLC_to_MAC"), 0);

        // store the node id of the owner module
        ownerNodeId_ = mac->getMacNodeId();

        // get the reference to the RLC module
        lteRlc_.reference(this, "umModule", true);
        queueSize_ = lteRlc_->par("queueSize");

    packetFlowObserver_.reference(this, "packetFlowObserverModule", false);

    // @author Alessandro Noferi
    if (mac->getNodeType() == NODEB) {
        if (packetFlowObserver_) {
            EV << "UmTxEntity::initialize - RLC layer is for a base station" << endl;
            ASSERT(check_and_cast<PacketFlowObserverEnb *>(packetFlowObserver_.get()));
        }
    }
    else if (mac->getNodeType() == UE) {
        if (packetFlowObserver_) {
            EV << "UmTxEntity::initialize - RLC layer, casting the packetFlowObserver " << endl;
            ASSERT(check_and_cast<PacketFlowObserverUe *>(packetFlowObserver_.get()));
        }
    }
        burstStatus_ = INACTIVE;
    }
}

bool UmTxEntity::enque(cPacket *pkt)
{
    EV << NOW << " UmTxEntity::enque - buffering new SDU  " << endl;
    if (queueSize_ == 0 || queueLength_ + pkt->getByteLength() < queueSize_) {
        // Buffer the SDU in the TX buffer
        sduQueue_.insert(pkt);
        queueLength_ += pkt->getByteLength();
        // Packet was successfully enqueued
        return true;
    }
    else {
        // Buffer is full - cannot enqueue packet
        return false;
    }
}

void UmTxEntity::rlcPduMake(int pduLength)
{
    EV << NOW << " UmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;

    // the request from MAC takes into account also the size of the RLC header
    pduLength -= RLC_HEADER_UM;

    int len = 0;
    inet::Packet *pktPdu = nullptr;
    auto rlcPduForPacketFlow = inet::makeShared<LteRlcUmDataPdu>(); // For PacketFlowManager compatibility

    bool startFrag = firstIsFragment_;
    bool endFrag = false;

    // NR-style: handle only one SDU per PDU (no concatenation)
    if (!sduQueue_.isEmpty() && pduLength > 0) {
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

        EV << NOW << " UmTxEntity::rlcPduMake - Processing SDU from the queue, sduSno[" << sduSequenceNumber
           << "], length[" << sduLength << "]" << endl;

        if (pduLength >= sduLength) {
            EV << NOW << " UmTxEntity::rlcPduMake - Add complete SDU " << sduLength << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            // Complete SDU: Reuse the original packet, just add RlcUmHeader at front
            if (fragmentInfo) {
                delete fragmentInfo;
                fragmentInfo = nullptr;
            }
            len = sduLength;

            pkt = check_and_cast<inet::Packet *>(sduQueue_.pop());
            queueLength_ -= pkt->getByteLength();

            // Reuse the packet, just add RLC header
            auto rlcHeader = inet::makeShared<RlcUmHeader>();
            FramingInfo fi;
            fi.firstIsFragment = startFrag;
            fi.lastIsFragment = false;
            rlcHeader->setFramingInfo(fi);
            rlcHeader->setPduSequenceNumber(sno_++);
            pkt->insertAtFront(rlcHeader);
            pktPdu = pkt; // reuse the original packet

            // For PacketFlowManager compatibility
            auto pktForPfm = pkt->dup();
            rlcPduForPacketFlow->setSdu(pktForPfm, sduLength);

            EV << NOW << " UmTxEntity::rlcPduMake - Reused complete SDU packet, sduSno[" << sduSequenceNumber << "]" << endl;

            // now, the first SDU in the buffer is not a fragment
            firstIsFragment_ = false;
        }
        else {
            EV << NOW << " UmTxEntity::rlcPduMake - Add fragment " << pduLength << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            // Fragment: Duplicate packet and replace data with SliceChunk
            len = pduLength;

            // Calculate fragment offset
            int fragmentOffset = 0;
            if (fragmentInfo != nullptr) {
                fragmentOffset = pdcpTag->getOriginalPacketLength() - fragmentInfo->size;
                fragmentInfo->size -= pduLength;
                if (fragmentInfo->size < 0)
                    throw cRuntimeError("Fragmentation error");
            }
            else {
                fragmentInfo = new FragmentInfo;
                fragmentInfo->pkt = pkt;
                fragmentInfo->size = sduLength - pduLength;
            }

            // Create fragment packet by duplicating original and replacing data with SliceChunk
            pktPdu = pkt->dup();
            auto originalData = pktPdu->removeData(); // remove all data chunks
            auto sliceChunk = inet::makeShared<inet::SliceChunk>(originalData, inet::b(fragmentOffset * 8), inet::b(pduLength * 8));
            pktPdu->insertAtBack(sliceChunk);

            // Add RLC header
            auto rlcHeader = inet::makeShared<RlcUmHeader>();
            FramingInfo fi;
            fi.firstIsFragment = startFrag;
            fi.lastIsFragment = true;
            rlcHeader->setFramingInfo(fi);
            rlcHeader->setPduSequenceNumber(sno_++);
            pktPdu->insertAtFront(rlcHeader);

            // For PacketFlowManager compatibility
            auto rlcSduDup = pkt->dup();
            rlcPduForPacketFlow->setSdu(rlcSduDup, pduLength);

            endFrag = true;

            EV << NOW << " UmTxEntity::rlcPduMake - SDU fragment created, remaining " << fragmentInfo->size << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            // now, the first SDU in the buffer is a fragment
            firstIsFragment_ = true;
        }
    }

    if (len == 0) {
        // send an empty (1-bit) message to notify the MAC that there is not enough space to send RLC PDU
        EV << NOW << " UmTxEntity::rlcPduMake - cannot send PDU with data, pdulength requested by MAC (" << pduLength << "B) is too small." << std::endl;
        pktPdu = new inet::Packet("lteRlcFragment (empty)");
        auto rlcHeader = inet::makeShared<RlcUmHeader>();
        rlcHeader->setChunkLength(inet::b(1)); // send only a bit, minimum size
        pktPdu->insertAtFront(rlcHeader);
    }

    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;

    /*
     * @author Alessandro Noferi
     *
     * Notify the packetFlowObserver about the new RLC PDU
     * only in UL or DL cases
     */
    if (flowControlInfo_->getDirection() == DL || flowControlInfo_->getDirection() == UL) {
        // add RLC PDU to packetFlowObserver
        if (len != 0 && packetFlowObserver_ != nullptr) {
            LogicalCid lcid = flowControlInfo_->getLcid();

            /*
             * Burst management.
             *
             * If the buffer is empty, the burst, if ACTIVE,
             * now is finished. Tell the flow manager to STOP
             * keep track of burst RLCs (not the timer). Set burst as INACTIVE
             *
             * if the buffer is NOT empty,
             *      if burst is already ACTIVE, do not start the timer T2
             *      if burst is INACTIVE, START the timer T2 and set it as ACTIVE
             * Tell the flow manager to keep track of burst RLCs
             */

            if (sduQueue_.isEmpty()) {
                if (burstStatus_ == ACTIVE) {
                    EV << NOW << " UmTxEntity::burstStatus - ACTIVE -> INACTIVE" << endl;

                    packetFlowObserver_->insertRlcPdu(lcid, rlcPduForPacketFlow, STOP);
                    burstStatus_ = INACTIVE;
                }
                else {
                    EV << NOW << " UmTxEntity::burstStatus - " << burstStatus_ << endl;

                    packetFlowObserver_->insertRlcPdu(lcid, rlcPduForPacketFlow, burstStatus_);
                }
            }
            else {
                if (burstStatus_ == INACTIVE) {
                    burstStatus_ = ACTIVE;
                    EV << NOW << " UmTxEntity::burstStatus - INACTIVE -> ACTIVE" << endl;
                    //start a new burst
                    packetFlowObserver_->insertRlcPdu(lcid, rlcPduForPacketFlow, START);
                }
                else {
                    EV << NOW << " UmTxEntity::burstStatus - burstStatus: " << burstStatus_ << endl;

                    // burst is still active
                    packetFlowObserver_->insertRlcPdu(lcid, rlcPduForPacketFlow, burstStatus_);
                }
            }
        }
    }

    // send to MAC layer
    EV << NOW << " UmTxEntity::rlcPduMake - send PDU with size " << pktPdu->getByteLength() << " bytes to lower layer" << endl;
    lteRlc_->sendToLowerLayer(pktPdu);

    // if incoming connection was halted
    if (notifyEmptyBuffer_ && sduQueue_.isEmpty()) {
        notifyEmptyBuffer_ = false;

        // tell the RLC UM to resume packets for the new mode
        lteRlc_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void UmTxEntity::dropBufferOverflow(cPacket *pkt)
{
    EV << "LteRlcUm : Dropping packet " << pkt->getName() << " (queue full) \n";
    delete pkt;
}

void UmTxEntity::removeDataFromQueue()
{
    EV << NOW << " UmTxEntity::removeDataFromQueue - removed SDU " << endl;

    // get the last packet...
    cPacket *pkt = sduQueue_.back();

    // ...and remove it
    cPacket *retPkt = sduQueue_.remove(pkt);
    queueLength_ -= retPkt->getByteLength();
    ASSERT(queueLength_ >= 0);
    delete retPkt;
}

void UmTxEntity::clearQueue()
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

bool UmTxEntity::isHoldingDownstreamInPackets()
{
    return holdingDownstreamInPackets_;
}

void UmTxEntity::enqueHoldingPackets(cPacket *pkt)
{
    EV << NOW << " UmTxEntity::enqueHoldingPackets - storing new SDU into the holding buffer " << endl;
    sduHoldingQueue_.insert(pkt);
}

void UmTxEntity::resumeDownstreamInPackets()
{
    EV << NOW << " UmTxEntity::resumeDownstreamInPackets - resume buffering incoming downstream packets of the RLC entity associated with the new mode" << endl;

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
            lteRlc_->sendToLowerLayer(pktRlcdup);
        }
        else {
            // Queue is full - drop SDU
            EV << "UmTxEntity::resumeDownstreamInPackets - cannot buffer SDU (queue is full), dropping" << std::endl;
            dropBufferOverflow(pktRlc);
        }
    }
}

void UmTxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {
            EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - clear TX buffer of the RLC entity associated with the old mode" << endl;
            clearQueue();
        }
        else {
            if (!sduQueue_.isEmpty()) {
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - check when the TX buffer of the RLC entity associated with the old mode becomes empty - queue length[" << sduQueue_.getLength() << "]" << endl;
                notifyEmptyBuffer_ = true;
            }
            else {
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - TX buffer of the RLC entity associated with the old mode is already empty" << endl;
            }
        }
    }
    else {
        EV << " UmTxEntity::rlcHandleD2DModeSwitch - reset numbering of the RLC TX entity corresponding to the new mode" << endl;
        sno_ = 0;

        if (!clearBuffer) {
            if (lteRlc_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId())) {
                // stop incoming connections, until
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - halt incoming downstream connections of the RLC entity associated with the new mode" << endl;
                startHoldingDownstreamInPackets();
            }
        }
    }
}

} //namespace
