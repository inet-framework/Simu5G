//
//                  Simu5G
//
// Authors: Esteban Egea Lopez (Universidad Politecnica de Cartagena), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/rlc/um/NrRlcUmTxEntity.h"
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/rrc/D2DModeController.h"

namespace simu5g {

Define_Module(NrRlcUmTxEntity);

using namespace inet;

void NrRlcUmTxEntity::initMode()
{
    sn_FieldLength = par("sn_FieldLength");
    // 3GPP UM SN field lengths: LTE (TS 36.322) 5 or 10 bits, NR (TS 38.322) 6 or 12 bits.
    if (sn_FieldLength != 5 && sn_FieldLength != 6 && sn_FieldLength != 10 && sn_FieldLength != 12)
        throw cRuntimeError("NrRlcUmTxEntity::initialize() sn_FieldLength=%u, but only 5, 6, 10 or 12 are valid", sn_FieldLength);
    sduBuffer = new RlcUmTransmitterBuffer(static_cast<uint32_t>(sn_FieldLength));
}

bool NrRlcUmTxEntity::storeSdu(inet::Packet *pkt)
{
    emit(receivedPacketFromUpperLayerSignal_, pkt);
    sduBuffer->addSdu(pkt->getByteLength(), pkt);
    return true;
}

void NrRlcUmTxEntity::rlcPduMake(int pduLength)
{
    Enter_Method("NrRlcUmTxEntity::rlcPduMake");
    EV << NOW << " NrRlcUmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;
    emit(requestedPduSizeSignal_, pduLength);

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
        EV << NOW << " NrRlcUmTxEntity::rlcPduMake - grant too small (" << pduLength << "B)" << endl;
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
        EV << NOW << " NrRlcUmTxEntity::rlcPduMake - buffer empty, wasting grant (" << size << "B)" << endl;
        emit(wastedGrantedBytesSignal_, size);
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

    rlcPdu->setChunkLength(inet::B(rlcHeader + len));
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
    EV << NOW << " NrRlcUmTxEntity::rlcPduMake - send PDU size " << pkt->getByteLength() << " to lower layer" << endl;
    emit(sentPduSizeSignal_, pkt->getByteLength());
    emit(sentPacketToLowerLayerSignal_, pkt);
    sendPduToMac(pkt);

    notifyControllerIfEmptied();
}

void NrRlcUmTxEntity::notifyControllerIfEmptied()
{
    // Once the old-mode entity has drained, release the new-mode entity's holding
    // buffer via the D2D controller (mode-switch handover of buffered SDUs).
    if (notifyEmptyBuffer_ && !sduBuffer->hasData()) {
        notifyEmptyBuffer_ = false;
        if (d2dModeController_ && flowControlInfo_)
            d2dModeController_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void NrRlcUmTxEntity::clearQueue()
{
    // empty the TX buffer (deletes buffered SDUs); the UM numbering is NOT reset
    sduBuffer->clearBuffer();
}

void NrRlcUmTxEntity::resumeDownstreamInPackets()
{
    EV << NOW << " NrRlcUmTxEntity::resumeDownstreamInPackets - releasing held SDUs to the TX buffer" << endl;
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

void NrRlcUmTxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    // This is reached by a direct call from RlcMux on a D2D mode switch; clearing the
    // old-mode buffer below deletes SDUs owned by THIS entity. Switch the execution
    // context to this entity so those deletes run in the owner's context, otherwise
    // OMNeT++ flags "deleting an object it doesn't own" (the LTE path avoids this via
    // cQueue::pop(), which releases ownership to the context before delete).
    Enter_Method_Silent("rlcHandleD2DModeSwitchNr");
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == NODEB) {
            EV << NOW << " NrRlcUmTxEntity::rlcHandleD2DModeSwitch - nothing to do on the DL leg of an IM flow" << endl;
            return;
        }
        if (clearBuffer) {
            EV << NOW << " NrRlcUmTxEntity::rlcHandleD2DModeSwitch - clear old-mode TX buffer" << endl;
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
