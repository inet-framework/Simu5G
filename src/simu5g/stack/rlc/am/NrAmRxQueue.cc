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

#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/NetworkInterface.h>

#include "NrAmRxQueue.h"
#include "NrAmTxQueue.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmDataPdu.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rrc/BearerManagement.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

Define_Module(NrAmRxQueue);

simsignal_t NrAmRxQueue::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t NrAmRxQueue::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");
simsignal_t NrAmRxQueue::rxWindowOccupationSignal_ = registerSignal("rxWindowOccupation");
simsignal_t NrAmRxQueue::rlcCellThroughputSignal_[2] = { registerSignal("rlcCellThroughputDl"), registerSignal("rlcCellThroughputUl") };

NrAmRxQueue::~NrAmRxQueue()
{
    delete rxBuffer_;
    cancelAndDelete(tReassemblyTimer_);
    cancelAndDelete(tStatusProhibitTimer_);
    delete ackFlowControlInfo_;
}

void NrAmRxQueue::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        nameEntity_ = getFullPath();

        amWindowSize_ = par("AM_Window_Size");
        if (amWindowSize_ != 2048 && amWindowSize_ != 131072)
            throw cRuntimeError("NrAmRxQueue::initialize() AM_Window_Size=%u, only 2048 or 131072 are valid", amWindowSize_);

        rxBuffer_ = new RlcSduSlidingWindowReceptionBuffer(amWindowSize_, nameEntity_ + "-rx-sliding window:");
        tReassemblyTimer_ = new cMessage("t_ReassemblyTimer");
        tReassembly_ = par("t_Reassembly");
        tStatusProhibitTimer_ = new cMessage("t_StatusProhibitTimer");
        tStatusProhibit_ = par("t_StatusProhibit");
    }
}

void NrAmRxQueue::enque(Packet *pkt)
{
    Enter_Method("enque()");
    take(pkt);

    auto pdu = pkt->peekAtFront<NrRlcAmDataPdu>();

    // Extract FlowControlInfo on first PDU (swap src/dst for status reports)
    if (ackFlowControlInfo_ == nullptr) {
        auto orig = pkt->getTag<FlowControlInfo>();
        ackFlowControlInfo_ = orig->dup();
        ackFlowControlInfo_->setSourceId(orig->getDestId());
        ackFlowControlInfo_->setDestId(orig->getSourceId());
        ackFlowControlInfo_->setDirection((orig->getDirection() == DL) ? UL : DL);
    }

    unsigned int sn = pdu->getPduSequenceNumber();

    // TS 38.322 5.2.3.2: discard PDUs outside the RX window
    if (!rxBuffer_->inWindow(sn)) {
        if (pdu->getPollStatus())
            sendStatusReport();
        delete pkt;
        return;
    }

    // Discard duplicate (already completed) PDUs
    if (rxBuffer_->isReady(sn)) {
        if (pdu->getPollStatus())
            sendStatusReport();
        delete pkt;
        return;
    }

    int totalSduLength = pdu->getLengthMainPacket();
    int start = pdu->getStartOffset();
    int end = pdu->getEndOffset();

    auto segmentResult = rxBuffer_->handleSegment(sn, totalSduLength, start, end, pkt);
    if (segmentResult.second) {
        // Duplicate segment — discard
        if (pdu->getPollStatus())
            sendStatusReport();
        delete pkt;
        return;
    }

    if (segmentResult.first) {
        // SDU fully reassembled
        passUp(sn);
        if (sn == rxBuffer_->getRxHighestStatus())
            rxBuffer_->updateRxHighestStatus();
        if (sn == rxBuffer_->getRxNext())
            rxBuffer_->updateRxNext();
    }

    // TS 38.322 5.3.4: polling
    if (pdu->getPollStatus()) {
        if (sn < rxBuffer_->getRxHighestStatus() || rxBuffer_->aboveWindow(sn))
            sendStatusReport();
    }

    // TS 38.322 5.2.3.2.3: reassembly timer management
    unsigned int currentRxNext = rxBuffer_->getRxNext();
    bool hasHoles = rxBuffer_->hasMissingByteSegmentBeforeLast(currentRxNext);

    if (tReassemblyTimer_->isScheduled()) {
        bool noHolesAndStatus = (currentRxNext + 1 == rxNextStatusTrigger_ && !hasHoles);
        bool statusOff = (!rxBuffer_->inWindow(rxNextStatusTrigger_)
                && rxNextStatusTrigger_ != rxBuffer_->getRxNext() + amWindowSize_);

        if (currentRxNext == rxNextStatusTrigger_ || noHolesAndStatus || statusOff)
            cancelEvent(tReassemblyTimer_);
    }

    // Not else: timer may have just been cancelled above
    if (!tReassemblyTimer_->isScheduled()) {
        bool missingAndHole = (rxBuffer_->getRxNextHighest() == currentRxNext + 1 && hasHoles);
        if (rxBuffer_->getRxNextHighest() > currentRxNext + 1 || missingAndHole) {
            EV << NOW << " NrAmRxQueue::enque() t_ReassemblyTimer scheduled" << endl;
            scheduleAfter(tReassembly_, tReassemblyTimer_);
            rxNextStatusTrigger_ = rxBuffer_->getRxNextHighest();
        }
    }
}

void NrAmRxQueue::passUp(int seqNum)
{
    Enter_Method("passUp");

    Packet *bufferedPkt = rxBuffer_->consumeSdu(seqNum);
    if (!bufferedPkt)
        throw cRuntimeError("NrAmRxQueue::passUp() null PDU for seqNum=%d", seqNum);

    auto pdu = bufferedPkt->removeAtFront<NrRlcAmDataPdu>();
    if (pdu->getNumSdu() < 1)
        throw cRuntimeError("NrAmRxQueue::passUp() PDU has no SDU");

    size_t sduLengthPktLen;
    auto sdu = pdu->popSdu(sduLengthPktLen);
    auto sduPdcp = sdu->getTag<PdcpTrackingTag>();
    EV << NOW << " NrAmRxQueue::passUp() SDU[" << sduPdcp->getPdcpSequenceNumber()
       << "] from PDU sn=" << seqNum << endl;

    // Restore the original (un-reversed) flow info for PDCP. ackFlowControlInfo_
    // has src/dst swapped (for status reports), so swap back here.
    Direction dir = (ackFlowControlInfo_->getDirection() == DL) ? UL : DL;
    MacNodeId srcId = ackFlowControlInfo_->getDestId();
    MacNodeId dstId = ackFlowControlInfo_->getSourceId();

    auto ci = sdu->addTagIfAbsent<FlowControlInfo>();
    ci->setSourceId(srcId);
    ci->setDestId(dstId);
    ci->setDirection(dir);
    ci->setDrbId(ackFlowControlInfo_->getDrbId());
    ci->setRlcType(AM);
    sdu->removeTagIfPresent<PdcpTrackingTag>();

    // Cell throughput at the RLC layer (DL when the original data flow was DL)
    totalRcvdBytes_ += sdu->getByteLength();
    double tput = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    emit(rlcCellThroughputSignal_[dir == DL ? 0 : 1], tput);

    emit(sentPacketToUpperLayerSignal_, sdu);
    send(sdu, "out");
    passedUpSdus_.insert(sduPdcp->getPdcpSequenceNumber());
    delete bufferedPkt;
}

void NrAmRxQueue::sendStatusReport()
{
    Enter_Method("sendStatusReport()");

    if (tStatusProhibitTimer_->isScheduled()) {
        EV << NOW << " NrAmRxQueue::sendStatusReport, minimum interval not reached "
           << tStatusProhibit_ << endl;
        statusReportPending_ = true;
        return;
    }

    StatusPduData data = rxBuffer_->generateStatusPduData();

    auto pktPdu = new Packet("NR STATUS AM PDU");
    auto pdu = makeShared<NrRlcAmStatusPdu>();
    pdu->setAmType(ACK);
    pdu->setData(data);

    // TODO: compute proper length per TS 38.322 6.2.2.5
    // Header(1) + ACK_SN(2) + E1 = 3 bytes base
    unsigned int size = 3;
    for (const auto &nack : data.nacks) {
        size++; // NACK_SN
        if (nack.isSegment)
            size += 4; // SOstart + SOend
        else if (nack.nackRange > 1)
            size++;
    }
    pdu->setChunkLength(B(size));

    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *ackFlowControlInfo_;
    pktPdu->insertAtFront(pdu);

    EV << NOW << " NrAmRxQueue::sendStatusReport. Last sent " << lastSentAck_ << endl;
    bufferControlViaTxEntity(pktPdu);
    lastSentAck_ = NOW;
    scheduleAfter(tStatusProhibit_, tStatusProhibitTimer_);
    statusReportPending_ = false;
}

void NrAmRxQueue::routeControlToTxEntity(Packet *pkt)
{
    // Received STATUS PDU: hand it to the co-located TX side of this AM entity
    // (ctrlOut is connected to tx.ctrlIn inside the RlcAmEntity compound). The TX
    // side always exists: bearers are established duplex.
    send(pkt, "ctrlOut");
}

void NrAmRxQueue::bufferControlViaTxEntity(Packet *pkt)
{
    // Locally generated STATUS report: hand it to the co-located TX side for
    // transmission on this bearer's logical channel (statusOut -> tx.statusIn).
    send(pkt, "statusOut");
}

void NrAmRxQueue::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == tReassemblyTimer_) {
            rxBuffer_->handleReassemblyTimerExpiry(rxNextStatusTrigger_);
            bool hasHoles = rxBuffer_->hasMissingByteSegmentBeforeLast(rxBuffer_->getRxHighestStatus());
            bool restart = (rxBuffer_->getRxNextHighest() == rxBuffer_->getRxHighestStatus() && hasHoles);

            if (rxBuffer_->getRxNextHighest() > rxBuffer_->getRxHighestStatus() + 1 || restart) {
                scheduleAfter(tReassembly_, tReassemblyTimer_);
                rxNextStatusTrigger_ = rxBuffer_->getRxNextHighest();
            }
            // TS 38.322 5.3.4
            sendStatusReport();
        }
        else if (msg == tStatusProhibitTimer_) {
            if (statusReportPending_)
                sendStatusReport();
        }
        return;
    }

    // PDU from the MAC (via the mux)
    auto pkt = check_and_cast<Packet *>(msg);
    auto chunk = pkt->peekAtFront<inet::Chunk>();
    if (inet::dynamicPtrCast<const NrRlcAmStatusPdu>(chunk) != nullptr) {
        // STATUS/ACK control PDU: route to the local AM TX entity
        routeControlToTxEntity(pkt);
    }
    else {
        emit(receivedPacketFromLowerLayerSignal_, pkt);
        enque(pkt);
    }
}

} //namespace
