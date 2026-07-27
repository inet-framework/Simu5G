//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include <inet/common/ProtocolTag_m.h>

#include "NrUmTxEntity.h"
#include "NrRlcUmDataPdu.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

Define_Module(NrUmTxEntity);

simsignal_t NrUmTxEntity::wastedGrantedBytes = registerSignal("wastedGrantedBytes");
simsignal_t NrUmTxEntity::requestedPDUSizeSignal = registerSignal("requestedPDUSize");
simsignal_t NrUmTxEntity::sentPDUSizeSignal = registerSignal("sentPDUSize");

NrUmTxEntity::~NrUmTxEntity()
{
    if (sduBuffer) {
        sduBuffer->clearBuffer();
        delete sduBuffer;
    }
}

void NrUmTxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        sn_FieldLength = par("sn_FieldLength");
        if (sn_FieldLength != 6 && sn_FieldLength != 12)
            throw cRuntimeError("NrUmTxEntity::initialize() sn_FieldLength=%u, but only 6 or 12 are valid", sn_FieldLength);
        sduBuffer = new RlcUmTransmitterBuffer(static_cast<uint32_t>(sn_FieldLength));
    }
}

void NrUmTxEntity::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);
    cGate *incoming = pkt->getArrivalGate();
    if (incoming->isName("in")) {
        handleSdu(pkt);
    }
    else if (incoming->isName("macIn")) {
        // Enter_Method + take needed for context switch when called via gate from the MAC mux
        Enter_Method("handleMacSduRequest()");
        if (pkt->getOwner() != this)
            take(pkt);
        auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
        rlcPduMake(macSduRequest->getSduSize());
        delete pkt;
    }
    else {
        throw cRuntimeError("NrUmTxEntity: unexpected message from gate %s", incoming->getFullName());
    }
}

void NrUmTxEntity::handleSdu(Packet *pkt)
{
    EV << NOW << " NrUmTxEntity::handleSdu - SDU from upper layer, size " << pkt->getByteLength() << "\n";

    // Add PDCP tracking information (SN + original length), read back in rlcPduMake().
    auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
    auto pdcpTag = pkt->addTag<PdcpTrackingTag>();
    pdcpTag->setPdcpSequenceNumber(pdcpHeader->getSequenceNumber());
    pdcpTag->setOriginalPacketLength(pkt->getByteLength());

    sduBuffer->addSdu(pkt->getByteLength(), pkt);

    // Notify the MAC that the queue has new data. The dup carries the full SDU
    // length, so the MAC tracks remaining bytes itself across the per-segment
    // grant requests — no per-fragment re-notification is needed.
    auto pktDup = pkt->dup();
    pktDup->addTag<LteRlcNewDataTag>();
    send(pktDup, "out");
}

void NrUmTxEntity::rlcPduMake(int pduLength)
{
    Enter_Method("NrUmTxEntity::rlcPduMake");
    EV << NOW << " NrUmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC" << endl;
    emit(requestedPDUSizeSignal, pduLength);

    auto pkt = new inet::Packet("lteRlcFragment");
    auto rlcPdu = inet::makeShared<NrRlcUmDataPdu>();

    // the request from MAC takes into account also the size of the RLC header
    int size = pduLength - RLC_HEADER_UM;

    if (size <= 0) {
        // send an empty (1-bit) message: not enough space to carry data
        EV << NOW << " NrUmTxEntity::rlcPduMake - grant too small (" << pduLength << "B)" << endl;
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
        EV << NOW << " NrUmTxEntity::rlcPduMake - buffer empty, wasting grant (" << size << "B)" << endl;
        emit(wastedGrantedBytes, size);
        delete pkt;
        return;
    }

    auto bufferedSdu = check_and_cast<inet::Packet *>(segment.ptr);
    auto pdcpTag = bufferedSdu->getTag<PdcpTrackingTag>();
    unsigned int sduSequenceNumber = pdcpTag->getPdcpSequenceNumber();
    int sduLength = pdcpTag->getOriginalPacketLength();
    rlcPdu->pushSdu(bufferedSdu->dup(), sduLength);
    unsigned int startOffset = segment.start;
    unsigned int endOffset = segment.end;
    bool endFragment = segment.isLastSegment;
    if (segment.isLastSegment || segment.isFull)
        delete bufferedSdu; // it was previously dup'd into the SDU buffer

    // compute SI (reusing the FramingInfo field; semantics per TS 38.322)
    FramingInfo fi;  // 00 = full SDU
    if (endFragment)
        fi.firstIsFragment = true;  // 10 = last segment

    int len = segment.end - segment.start + 1;
    rlcPdu->setChunkLength(inet::B(RLC_HEADER_UM + len));
    rlcPdu->setSnoMainPacket(sduSequenceNumber);
    rlcPdu->setLengthMainPacket(0);
    rlcPdu->setStartOffset(startOffset);
    rlcPdu->setEndOffset(endOffset);
    rlcPdu->setFramingInfo(fi);
    // Full SDUs do not need a SN
    if (!segment.isFull)
        rlcPdu->setPduSequenceNumber(segment.sn);

    if (flowControlInfo_)
        *pkt->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;

    pkt->insertAtFront(rlcPdu);
    EV << NOW << " NrUmTxEntity::rlcPduMake - send PDU size " << pkt->getByteLength() << " to lower layer" << endl;
    emit(sentPDUSizeSignal, pkt->getByteLength());
    sendPduToMac(pkt);
}

void NrUmTxEntity::sendPduToMac(inet::Packet *pkt)
{
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    send(pkt, "out");
}

} //namespace
