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
#include <inet/networklayer/common/NetworkInterface.h>

#include "simu5g/stack/rlc/um/RlcUmTxEntity.h"
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"

#include "simu5g/stack/packetFlowObserver/PacketFlowSignals.h"
#include "simu5g/stack/rrc/D2DModeController.h"

namespace simu5g {

Define_Module(RlcUmTxEntity);

simsignal_t RlcUmTxEntity::rlcPduCreatedSignal_ = registerSignal("rlcPduCreated");
simsignal_t RlcUmTxEntity::wastedGrantedBytes = registerSignal("wastedGrantedBytes");
simsignal_t RlcUmTxEntity::requestedPDUSizeSignal = registerSignal("requestedPDUSize");
simsignal_t RlcUmTxEntity::sentPDUSizeSignal = registerSignal("sentPDUSize");

using namespace inet;

/*
 * Main functions
 */

void RlcUmTxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        soFraming_ = par("soFraming");

        LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
        ownerNodeId_ = mac->getMacNodeId();

        if (soFraming_) {
            sn_FieldLength = par("sn_FieldLength");
            // 3GPP UM SN field lengths: LTE (TS 36.322) 5 or 10 bits, NR (TS 38.322) 6 or 12 bits.
            if (sn_FieldLength != 5 && sn_FieldLength != 6 && sn_FieldLength != 10 && sn_FieldLength != 12)
                throw cRuntimeError("RlcUmTxEntity::initialize() sn_FieldLength=%u, but only 5, 6, 10 or 12 are valid", sn_FieldLength);
            sduBuffer = new RlcUmTransmitterBuffer(static_cast<uint32_t>(sn_FieldLength));
        }
        else {
            queueSize_ = par("queueSize");
            burstStatus_ = INACTIVE;
        }

        auto *rrc = inet::getContainingNicModule(this)->getSubmodule("rrc");
        d2dModeController_ = dynamic_cast<D2DModeController *>(rrc ? rrc->getSubmodule("d2dModeController") : nullptr);
    }
}

void RlcUmTxEntity::setFlowControlInfo(FlowControlInfo *info)
{
    RlcTxEntityBase::setFlowControlInfo(info);
    // Record this flow's wire format on the (dup'd) flow info; it rides on every
    // packet tagged for the MAC, so the scheduler/MAC can multiplex multiple SO
    // PDUs into one grant (NR keeps one SDU/segment per PDU, no RLC concatenation).
    if (flowControlInfo_) {
        flowControlInfo_->setSoFraming(soFraming_);
        flowControlInfo_->setRlcSnFieldLength(sn_FieldLength);
    }
}

void RlcUmTxEntity::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *incoming = pkt->getArrivalGate();
    if (incoming->isName("in")) {
        handleSdu(pkt);
    }
    else if (incoming->isName("macIn")) {
        handleMacSduRequest(pkt);
    }
    else {
        throw cRuntimeError("RlcUmTxEntity: unexpected message from gate %s", incoming->getFullName());
    }
}

void RlcUmTxEntity::handleSdu(inet::Packet *pkt)
{
    EV << NOW << " RlcUmTxEntity::handleSdu - Received SDU from upper layer, size " << pkt->getByteLength() << "\n";

    // Add PDCP tracking information (SN + original length), read back in rlcPduMake().
    auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
    auto pdcpTag = pkt->addTag<PdcpTrackingTag>();
    pdcpTag->setPdcpSequenceNumber(pdcpHeader->getSequenceNumber());
    pdcpTag->setOriginalPacketLength(pkt->getByteLength());

    if (holdingDownstreamInPackets_) {
        // D2D mode switch in progress on the new-mode entity: hold the SDU and
        // do not notify the MAC until the old-mode entity has drained.
        EV << "RlcUmTxEntity::handleSdu - Enqueue packet into the Holding Buffer\n";
        enqueHoldingPackets(pkt);
        return;
    }

    bool stored;
    if (soFraming_) {
        sduBuffer->addSdu(pkt->getByteLength(), pkt);
        stored = true;
    }
    else {
        stored = enque(pkt);
    }

    if (stored) {
        // Notify the MAC that the queue has new data. The dup carries the full SDU
        // length, so the MAC tracks remaining bytes itself across grant requests.
        auto pktDup = pkt->dup();
        pktDup->addTag<LteRlcNewDataTag>();
        // This indication creates/updates the MAC outgoing connection; stamp the
        // flow's wire format on it so the scheduler multiplexes multiple SO PDUs
        // per grant (NR keeps one SDU/segment per PDU, no RLC concatenation).
        pktDup->getTagForUpdate<FlowControlInfo>()->setSoFraming(soFraming_);
        pktDup->getTagForUpdate<FlowControlInfo>()->setRlcSnFieldLength(sn_FieldLength);
        EV << "RlcUmTxEntity::handleSdu - Sending new data indication to MAC\n";
        send(pktDup, "out");
    }
    else {
        // Queue is full - drop SDU (LTE mode only; NR buffer is unbounded)
        dropBufferOverflow(pkt);
    }
}

void RlcUmTxEntity::handleMacSduRequest(inet::Packet *pkt)
{
    // Enter_Method + take needed for context switch when called via gate from the lower mux
    Enter_Method("handleMacSduRequest()");
    if (pkt->getOwner() != this)
        take(pkt);

    auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
    unsigned int size = macSduRequest->getSduSize();

    EV << NOW << " RlcUmTxEntity::handleMacSduRequest - MAC requests PDU of size " << size << "\n";

    // do segmentation/concatenation and send a PDU to the lower layer
    rlcPduMake(size);

    delete pkt;
}

bool RlcUmTxEntity::enque(cPacket *pkt)
{
    EV << NOW << " RlcUmTxEntity::enque - buffering new SDU  " << endl;
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

void RlcUmTxEntity::rlcPduMake(int pduSize)
{
    if (soFraming_)
        rlcPduMakeNr(pduSize);
    else
        rlcPduMakeLte(pduSize);
}

void RlcUmTxEntity::rlcPduMakeLte(int pduLength)
{
    EV << NOW << " RlcUmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;

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

        EV << NOW << " RlcUmTxEntity::rlcPduMake - Next data chunk from the queue, sduSno[" << sduSequenceNumber
           << "], length[" << sduLength << "]" << endl;

        if (pduLength >= sduLength) {
            EV << NOW << " RlcUmTxEntity::rlcPduMake - Add " << sduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

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

            EV << NOW << " RlcUmTxEntity::rlcPduMake - Pop data chunk from the queue, sduSno[" << sduSequenceNumber << "]" << endl;

            // now, the first SDU in the buffer is not a fragment
            firstIsFragment_ = false;

            EV << NOW << " RlcUmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;
        }
        else {
            EV << NOW << " RlcUmTxEntity::rlcPduMake - Add " << pduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

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

            EV << NOW << " RlcUmTxEntity::rlcPduMake - Data chunk in the queue is now " << newLength << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            pduLength = 0;

            // now, the first SDU in the buffer is a fragment
            firstIsFragment_ = true;

            EV << NOW << " RlcUmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;
        }
    }

    if (len == 0) {
        // send an empty (1-bit) message to notify the MAC that there is not enough space
        EV << NOW << " RlcUmTxEntity::rlcPduMake - cannot send PDU with data, pdulength requested by MAC (" << pduLength << "B) is too small." << std::endl;
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
                    EV << NOW << " RlcUmTxEntity::burstStatus - ACTIVE -> INACTIVE" << endl;

                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), STOP);
                    emit(rlcPduCreatedSignal_, &info);
                    burstStatus_ = INACTIVE;
                }
                else {
                    EV << NOW << " RlcUmTxEntity::burstStatus - " << burstStatus_ << endl;

                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), burstStatus_);
                    emit(rlcPduCreatedSignal_, &info);
                }
            }
            else {
                if (burstStatus_ == INACTIVE) {
                    burstStatus_ = ACTIVE;
                    EV << NOW << " RlcUmTxEntity::burstStatus - INACTIVE -> ACTIVE" << endl;
                    //start a new burst
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), START);
                    emit(rlcPduCreatedSignal_, &info);
                }
                else {
                    EV << NOW << " RlcUmTxEntity::burstStatus - burstStatus: " << burstStatus_ << endl;

                    // burst is still active
                    RlcPduSignalInfo info(drbKey, rlcPdu.get(), burstStatus_);
                    emit(rlcPduCreatedSignal_, &info);
                }
            }
        }
    }

    // send to MAC layer
    pkt->insertAtFront(rlcPdu);
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    EV << NOW << " RlcUmTxEntity::rlcPduMake - send PDU " << rlcPdu->getPduSequenceNumber() << " with size " << pkt->getByteLength() << " bytes to lower layer" << endl;
    send(pkt, "out");

    // if incoming connection was halted
    if (notifyEmptyBuffer_ && sduQueue_.isEmpty()) {
        notifyEmptyBuffer_ = false;
        // tell the D2D mode controller to resume packets for the new mode
        if (d2dModeController_)
            d2dModeController_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void RlcUmTxEntity::rlcPduMakeNr(int pduLength)
{
    Enter_Method("RlcUmTxEntity::rlcPduMakeNr");
    EV << NOW << " RlcUmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;
    emit(requestedPDUSizeSignal, pduLength);

    auto pkt = new inet::Packet("lteRlcFragment");
    auto rlcPdu = inet::makeShared<NrRlcUmDataPdu>();

    // The MAC request is the PDU budget (header + payload). The header is segment-state
    // dependent (TS 38.322 6.2.1.3): peek the head SDU to choose it before carving so the
    // payload fills the budget exactly, and so it matches the header the scheduler reserved
    // for this same PDU (nrUmHeaderBytes, drain-synced).
    unsigned int rlcHeader = RLC_HEADER_UM;
    uint32_t remaining, nextByte;
    if (sduBuffer->peekHead(remaining, nextByte)) {
        NrUmSegState st = (nextByte > 0) ? NRUM_CONTINUATION
                        : (remaining + 1 <= (uint32_t)pduLength) ? NRUM_COMPLETE
                        : NRUM_FIRST;
        rlcHeader = nrUmHeaderBytes(st, sn_FieldLength);
    }
    int size = pduLength - (int)rlcHeader;

    if (size <= 0) {
        // send an empty (1-bit) message: not enough space to carry data
        EV << NOW << " RlcUmTxEntity::rlcPduMake - grant too small (" << pduLength << "B)" << endl;
        pkt->setName("lteRlcFragment (empty)");
        rlcPdu->setChunkLength(inet::b(1));
        if (flowControlInfo_)
            *(pkt->addTagIfAbsent<FlowControlInfo>()) = *flowControlInfo_;
        pkt->insertAtFront(rlcPdu);
        sendPduToMac(pkt);
        return;
    }

    PendingSegmentUM segment = sduBuffer->getSegmentForGrant(size);
    if (!segment.isValid) {
        EV << NOW << " RlcUmTxEntity::rlcPduMake - buffer empty, wasting grant (" << size << "B)" << endl;
        emit(wastedGrantedBytes, size);
        delete pkt;
        notifyControllerIfEmptied();
        return;
    }

    auto bufferedSdu = check_and_cast<inet::Packet *>(segment.ptr);
    auto pdcpTag = bufferedSdu->getTag<PdcpTrackingTag>();
    unsigned int sduSequenceNumber = pdcpTag->getPdcpSequenceNumber();
    int sduLength = pdcpTag->getOriginalPacketLength();
    rlcPdu->pushSdu(bufferedSdu->dup(), sduLength);
    unsigned int startOffset = segment.start;
    unsigned int endOffset = segment.end;
    if (segment.isLastSegment || segment.isFull)
        delete bufferedSdu; // it was previously dup'd into the SDU buffer

    // A whole SDU carried in a single PDU covers [0, totalLength-1] => complete (SI=00).
    bool complete = (segment.start == 0 && segment.end == segment.totalLength - 1);
    int len = segment.end - segment.start + 1;

    // FramingInfo encodes the SI field; toValue() = firstIsFragment*2 | lastIsFragment.
    // TS 38.322 6.2.3.4 SI: 00 complete / 01 first / 10 last / 11 middle.
    // firstIsFragment <=> the Data field does not start at the SDU's first byte;
    // lastIsFragment  <=> it does not end at the SDU's last byte.
    FramingInfo fi;
    fi.firstIsFragment = (segment.start > 0);
    fi.lastIsFragment  = (segment.end < segment.totalLength - 1);
    // A complete SDU carries no SN (TS 38.322 6.2.1.3); segments carry the SDU SN.
    bool carrySn = !complete;
    // Carry the true SDU length so the RX detects byte-coverage completion
    // (TS 38.322 5.2.2.2.3). A complete SDU needs no reassembly.
    unsigned int lengthMainPacket = complete ? 0 : segment.totalLength;

    rlcPdu->setChunkLength(inet::B(RLC_HEADER_UM + len));
    rlcPdu->setSnoMainPacket(sduSequenceNumber);
    rlcPdu->setLengthMainPacket(lengthMainPacket);
    rlcPdu->setStartOffset(startOffset);
    rlcPdu->setEndOffset(endOffset);
    rlcPdu->setFramingInfo(fi);
    if (carrySn)
        rlcPdu->setPduSequenceNumber(segment.sn);

    if (flowControlInfo_)
        *pkt->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;

    pkt->insertAtFront(rlcPdu);
    EV << NOW << " RlcUmTxEntity::rlcPduMake - send PDU size " << pkt->getByteLength() << " to lower layer" << endl;
    emit(sentPDUSizeSignal, pkt->getByteLength());
    sendPduToMac(pkt);

    notifyControllerIfEmptied();
}

void RlcUmTxEntity::sendPduToMac(inet::Packet *pkt)
{
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    send(pkt, "out");
}

void RlcUmTxEntity::notifyControllerIfEmptied()
{
    // Once the old-mode entity has drained, release the new-mode entity's holding
    // buffer via the D2D controller (mode-switch handover of buffered SDUs).
    if (notifyEmptyBuffer_ && !sduBuffer->hasData()) {
        notifyEmptyBuffer_ = false;
        if (d2dModeController_ && flowControlInfo_)
            d2dModeController_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void RlcUmTxEntity::dropBufferOverflow(cPacket *pkt)
{
    EV << "RlcUmTxEntity : Dropping packet " << pkt->getName() << " (queue full) \n";
    delete pkt;
}

void RlcUmTxEntity::removeDataFromQueue()
{
    EV << NOW << " RlcUmTxEntity::removeDataFromQueue - removed SDU " << endl;
    cPacket *pkt = sduQueue_.back();
    cPacket *retPkt = sduQueue_.remove(pkt);
    queueLength_ -= retPkt->getByteLength();
    ASSERT(queueLength_ >= 0);
    delete retPkt;
}

void RlcUmTxEntity::clearQueue()
{
    if (soFraming_)
        clearQueueNr();
    else
        clearQueueLte();
}

void RlcUmTxEntity::clearQueueLte()
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

void RlcUmTxEntity::clearQueueNr()
{
    // empty the TX buffer (deletes buffered SDUs); the UM numbering is NOT reset
    sduBuffer->clearBuffer();
}

bool RlcUmTxEntity::isHoldingDownstreamInPackets()
{
    return holdingDownstreamInPackets_;
}

void RlcUmTxEntity::enqueHoldingPackets(cPacket *pkt)
{
    EV << NOW << " RlcUmTxEntity::enqueHoldingPackets - storing new SDU into the holding buffer " << endl;
    sduHoldingQueue_.insert(pkt);
}

void RlcUmTxEntity::resumeDownstreamInPackets()
{
    if (soFraming_)
        resumeDownstreamInPacketsNr();
    else
        resumeDownstreamInPacketsLte();
}

void RlcUmTxEntity::resumeDownstreamInPacketsLte()
{
    EV << NOW << " RlcUmTxEntity::resumeDownstreamInPackets - resume buffering incoming downstream packets of the RLC entity associated with the new mode" << endl;

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
            EV << "RlcUmTxEntity::resumeDownstreamInPackets - cannot buffer SDU (queue is full), dropping" << std::endl;
            dropBufferOverflow(pktRlc);
        }
    }
}

void RlcUmTxEntity::resumeDownstreamInPacketsNr()
{
    EV << NOW << " RlcUmTxEntity::resumeDownstreamInPackets - releasing held SDUs to the TX buffer" << endl;
    holdingDownstreamInPackets_ = false;

    while (!sduHoldingQueue_.isEmpty()) {
        auto pktRlc = check_and_cast<inet::Packet *>(sduHoldingQueue_.front());
        sduHoldingQueue_.pop();

        // store the SDU in the TX buffer (ownership transferred)
        sduBuffer->addSdu(pktRlc->getByteLength(), pktRlc);

        // notify the MAC of new data (dup carries the full SDU length)
        auto pktRlcdup = pktRlc->dup();
        pktRlcdup->addTag<LteRlcNewDataTag>();
        send(pktRlcdup, "out");
    }
}

void RlcUmTxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    if (soFraming_)
        rlcHandleD2DModeSwitchNr(oldConnection, clearBuffer);
    else
        rlcHandleD2DModeSwitchLte(oldConnection, clearBuffer);
}

void RlcUmTxEntity::rlcHandleD2DModeSwitchLte(bool oldConnection, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {
            EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - clear TX buffer of the RLC entity associated with the old mode" << endl;
            clearQueue();
        }
        else {
            if (!sduQueue_.isEmpty()) {
                EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - check when the TX buffer of the old mode becomes empty - queue length[" << sduQueue_.getLength() << "]" << endl;
                notifyEmptyBuffer_ = true;
            }
            else {
                EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - TX buffer of the old mode is already empty" << endl;
            }
        }
    }
    else {
        EV << " RlcUmTxEntity::rlcHandleD2DModeSwitch - reset numbering of the RLC TX entity corresponding to the new mode" << endl;
        sno_ = 0;

        if (!clearBuffer) {
            if (d2dModeController_ && d2dModeController_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId())) {
                EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - halt incoming downstream connections of the new mode" << endl;
                startHoldingDownstreamInPackets();
            }
        }
    }
}

void RlcUmTxEntity::rlcHandleD2DModeSwitchNr(bool oldConnection, bool clearBuffer)
{
    // This is reached by a direct call from RlcMux on a D2D mode switch; clearing the
    // old-mode buffer below deletes SDUs owned by THIS entity. Switch the execution
    // context to this entity so those deletes run in the owner's context, otherwise
    // OMNeT++ flags "deleting an object it doesn't own" (the LTE path avoids this via
    // cQueue::pop(), which releases ownership to the context before delete).
    Enter_Method_Silent("rlcHandleD2DModeSwitchNr");
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - nothing to do on the DL leg of an IM flow" << endl;
            return;
        }
        if (clearBuffer) {
            EV << NOW << " RlcUmTxEntity::rlcHandleD2DModeSwitch - clear old-mode TX buffer" << endl;
            clearQueue();
        }
        else if (sduBuffer->hasData()) {
            // keep draining; signal the controller once the buffer empties
            notifyEmptyBuffer_ = true;
        }
    }
    else {
        // new-mode entity: reset UM numbering
        sduBuffer->resetTxNext();
        if (!clearBuffer && d2dModeController_ && flowControlInfo_ &&
                d2dModeController_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId())) {
            // the old-mode entity is still draining: hold incoming SDUs until it finishes
            startHoldingDownstreamInPackets();
        }
    }
}

} //namespace
