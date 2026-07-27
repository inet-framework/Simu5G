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

#include "NrUmRxEntity.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

Define_Module(NrUmRxEntity);

simsignal_t NrUmRxEntity::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t NrUmRxEntity::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");
simsignal_t NrUmRxEntity::rlcCellThroughputSignal_[2] = { registerSignal("rlcCellThroughputDl"), registerSignal("rlcCellThroughputUl") };

NrUmRxEntity::~NrUmRxEntity()
{
    cancelAndDelete(t_ReassemblyTimer);
    if (sduBuffer) {
        sduBuffer->clearBuffer();
        delete sduBuffer;
    }
}

void NrUmRxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        UM_Window_Size = par("UM_Window_Size");
        if (UM_Window_Size == 2048)
            sduBuffer = new RlcUmReceptionBuffer(12);
        else if (UM_Window_Size == 32)
            sduBuffer = new RlcUmReceptionBuffer(6);
        else
            throw cRuntimeError("NrUmRxEntity::initialize() UM_Window_Size=%d, but only 2048 or 32 are valid", UM_Window_Size);

        t_ReassemblyTimer = new cMessage("UM Reassembly timer");
        t_Reassembly = par("t_Reassembly");
    }
}

void NrUmRxEntity::handleMessage(cMessage *msg)
{
    if (msg == t_ReassemblyTimer) {
        sduBuffer->onTimerExpiry();
        // restart the timer if there is still something pending
        if (sduBuffer->startTimer())
            scheduleAfter(t_Reassembly, t_ReassemblyTimer);
        return;
    }

    auto pkt = check_and_cast<Packet *>(msg);
    if (pkt->getArrivalGate() && pkt->getArrivalGate()->isName("in")) {
        emit(receivedPacketFromLowerLayerSignal_, pkt);
        enque(pkt);
    }
    else {
        throw cRuntimeError("NrUmRxEntity: unexpected message %s", msg->getFullName());
    }
}

void NrUmRxEntity::enque(Packet *pktPdu)
{
    EV << NOW << " NrUmRxEntity::enque - buffering new PDU" << endl;

    auto pdu = pktPdu->removeAtFront<NrRlcUmDataPdu>();

    // A full SDU (FI=00) can be delivered immediately; a segment goes to the
    // reassembly buffer. In NR UM the whole SDU is carried in every segment, so
    // the buffer can reassemble from the last received segment alone.
    FramingInfo fi = pdu->getFramingInfo();
    if (fi.toValue() == 0) {
        size_t sduLength;
        auto pktSdu = check_and_cast<Packet *>(pdu->popSdu(sduLength));
        toPdcp(pktSdu);
    }
    else {
        unsigned int tsn = pdu->getPduSequenceNumber();
        handlePDUInReceivedBuffer(pdu, tsn);
    }

    pktPdu->insertAtFront(pdu);
    delete pktPdu;
}

void NrUmRxEntity::handlePDUInReceivedBuffer(inet::Ptr<NrRlcUmDataPdu> pdu, unsigned int tsn)
{
    size_t totalLength = pdu->getLengthMainPacket();
    auto sdu = check_and_cast<Packet *>(pdu->popSdu(totalLength));

    bool complete = sduBuffer->handleSegment(tsn, totalLength, pdu->getStartOffset(), pdu->getEndOffset(), sdu);
    if (complete) // otherwise the SDU is buffered or was discarded
        toPdcp(sdu);

    if (t_ReassemblyTimer->isScheduled()) {
        if (sduBuffer->stopTimer(true))
            cancelEvent(t_ReassemblyTimer);
    }

    // not else: the timer may have been cancelled just above
    if (!t_ReassemblyTimer->isScheduled()) {
        if (sduBuffer->startTimer())
            scheduleAfter(t_Reassembly, t_ReassemblyTimer);
    }
}

void NrUmRxEntity::toPdcp(Packet *pktAux)
{
    // Set the receive-side flow info for PDCP routing and drop the TX-internal tag.
    *pktAux->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;
    pktAux->removeTagIfPresent<PdcpTrackingTag>();

    Direction dir = static_cast<Direction>(flowControlInfo_->getDirection());

    // Cell throughput at the RLC layer (emitted on this entity)
    totalRcvdBytes_ += pktAux->getByteLength();
    double tput = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    emit(rlcCellThroughputSignal_[dir == UL ? 1 : 0], tput);

    emit(sentPacketToUpperLayerSignal_, pktAux);

    EV << NOW << " NrUmRxEntity::toPdcp - deliver SDU of " << pktAux->getByteLength() << " bytes to upper layer" << endl;
    send(pktAux, "out");
}

} //namespace
