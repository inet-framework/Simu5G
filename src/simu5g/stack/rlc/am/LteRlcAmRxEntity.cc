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

#include "simu5g/stack/rlc/am/LteRlcAmRxEntity.h"
#include "simu5g/stack/rlc/am/RlcAmTxEntityBase.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

namespace simu5g {

Define_Module(LteRlcAmRxEntity);

using namespace inet;

unsigned int LteRlcAmRxEntity::totalCellRcvdBytes_ = 0;

LteRlcAmRxEntity::LteRlcAmRxEntity() :
    timer_(this)
{
    timer_.setTimerId(BUFFERSTATUS_T);
}

LteRlcAmRxEntity::~LteRlcAmRxEntity()
{
    for (int i = 0; i < pduBuffer_.size(); i++) {
        if (pduBuffer_.get(i) != nullptr)
            delete check_and_cast<Packet *>(pduBuffer_.remove(i));
    }
    for (auto& p : pendingPduBuffer_)
        delete p;
    pendingPduBuffer_.clear();
}

void LteRlcAmRxEntity::initMode()
{
    rxWindowDesc_.windowSize_ = par("rxWindowSize");
    ackReportInterval_ = par("ackReportInterval");
    statusReportInterval_ = par("statusReportInterval");

    discarded_.resize(rxWindowDesc_.windowSize_);
    received_.resize(rxWindowDesc_.windowSize_);
    binder_.reference(this, "binderModule", true);

    LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
    dir_ = mac->getNodeType() == NODEB ? UL : DL;
}

void LteRlcAmRxEntity::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        EV << NOW << "LteRlcAmRxEntity::handleMessage timer event received, sending status report " << endl;

        timer_.handle();

        sendStatusReportLte();

        // Autonomously advance the receiver window over the contiguous head of
        // received PDUs (replaces the MRW handshake).
        if (lastSentAck_ == NOW) {
            int shift = computeWindowShift();
            if (shift > 0)
                moveRxWindow(rxWindowDesc_.firstSeqNum_ + shift);
        }

        for (unsigned int i = 0; i < rxWindowDesc_.windowSize_; i++) {
            if (pduBuffer_.get(i) != nullptr) {
                timer_.start(statusReportInterval_);
                break;
            }
        }

        delete msg;
    }
    else {
        cGate *incoming = msg->getArrivalGate();
        if (incoming->isName("in")) {
            auto pkt = check_and_cast<Packet *>(msg);
            enque(pkt);
        }
        else {
            throw cRuntimeError("LteRlcAmRxEntity: unexpected message from gate %s", incoming->getFullName());
        }
    }
}

inet::Packet *LteRlcAmRxEntity::defragmentFrames(std::deque<Packet *>& fragmentFrames)
{
    EV_DEBUG << "Defragmenting " << fragmentFrames.size() << " fragments.\n";
    auto defragmentedFrame = new Packet();
    defragmentedFrame->copyTags(*(fragmentFrames.at(0)));

    std::string defragmentedName(fragmentFrames.at(0)->getName());
    auto index = defragmentedName.find("-frag");
    if (index != std::string::npos)
        defragmentedFrame->setName(defragmentedName.substr(0, index).c_str());

    for (auto fragmentFrame : fragmentFrames) {
        fragmentFrame->popAtFront<LteRlcAmPdu>();
        defragmentedFrame->insertAtBack(fragmentFrame->peekData());
        delete fragmentFrame;
    }

    fragmentFrames.clear();

    EV_TRACE << "Created " << *defragmentedFrame << ".\n";

    return defragmentedFrame;
}

void LteRlcAmRxEntity::discard(const int sn)
{
    int index = sn - rxWindowDesc_.firstSeqNum_;

    if ((index < 0) || (index >= rxWindowDesc_.windowSize_))
        throw cRuntimeError("LteRlcAmRxEntity::discard PDU %d out of rx window ", sn);

    int discarded = 0;

    Direction dir = UNKNOWN_DIRECTION;
    MacNodeId dstId = NODEID_NONE, srcId = NODEID_NONE;

    for (int i = 0; i <= index; ++i) {
        discarded_.at(i) = true;

        if (pduBuffer_.get(i) != nullptr) {
            auto pkt = check_and_cast<inet::Packet *>(pduBuffer_.remove(i));
            auto pdu = pkt->peekAtFront<LteRlcAmPdu>();
            auto ci = pdu->getTag<FlowControlInfo>();
            dir = ci->getDirection();
            dstId = ci->getDestId();
            srcId = ci->getSourceId();
            auto it = std::find(pendingPduBuffer_.begin(), pendingPduBuffer_.end(), pkt);
            if (it != pendingPduBuffer_.end())
                pendingPduBuffer_.erase(it);

            delete pkt;
            ++discarded;
        }
        else
            throw cRuntimeError("LteRlcAmRxEntity::discard PDU at position %d already discarded", i);
    }

    EV << NOW << " LteRlcAmRxEntity::discard , discarded " << discarded << " PDUs " << endl;

    if (dir != UNKNOWN_DIRECTION) {
        cModule *ue = binder_->getRlcByNodeId((dir == DL ? dstId : srcId));
        if (ue != nullptr)
            ue->emit(rlcPacketLossSignal_[dir_], 1.0);

        cModule *nodeb = binder_->getRlcByNodeId((dir == DL ? srcId : dstId));
        if (nodeb != nullptr)
            nodeb->emit(rlcCellPacketLossSignal_[dir_], 1.0);
    }
}

void LteRlcAmRxEntity::enque(Packet *pkt)
{
    auto pdu = pkt->peekAtFront<LteRlcAmPdu>();

    if (pdu->getAmType() != DATA) {
        if ((pdu->getAmType() == ACK)) {
            EV << NOW << " LteRlcAmRxEntity::enque Received ACK message" << endl;
            routeControlToTxEntityLte(pkt);
        }
        else {
            throw cRuntimeError("RLC AM - Unknown status PDU");
        }
        return;
    }

    if (timer_.idle()) {
        EV << NOW << " LteRlcAmRxEntity::enque reporting timer was idle, will fire at " << NOW.dbl() + statusReportInterval_.dbl() << endl;
        timer_.start(statusReportInterval_);
    }
    else {
        EV << NOW << " LteRlcAmRxEntity::enque reporting timer was already on, will fire at " << NOW.dbl() + timer_.remaining().dbl() << endl;
    }

    if (ackFlowControlInfo_ == nullptr) {
        auto orig = pkt->getTag<FlowControlInfo>();
        ackFlowControlInfo_ = orig->dup();
        ackFlowControlInfo_->setSourceId(orig->getDestId());
        ackFlowControlInfo_->setDestId(orig->getSourceId());
        ackFlowControlInfo_->setDirection((orig->getDirection() == DL) ? UL : DL);
    }

    int tsn = pdu->getSnoFragment();

    int index = tsn - rxWindowDesc_.firstSeqNum_;

    if (index < 0) {
        EV << NOW << " LteRlcAmRxEntity::enque the received PDU with " << index << " is below the RX window (already received), discarding" << endl;
        delete pkt;
    }
    else if ((index >= rxWindowDesc_.windowSize_)) {
        throw cRuntimeError("LteRlcAmRxEntity::enque(): received PDU with position %d out of the window of size %d", index, rxWindowDesc_.windowSize_);
    }
    else {
        if (tsn == rxWindowDesc_.seqNum_) {
            rxWindowDesc_.seqNum_++;
            EV << NOW << "LteRlcAmRxEntity::enque DATA PDU received at index [" << index << "] with fragment number [" << tsn << "] in sequence " << endl;
        }
        else {
            rxWindowDesc_.seqNum_ = tsn + 1;
            EV << NOW << "LteRlcAmRxEntity::enque DATA PDU received at index [" << index << "] with fragment number ["
               << tsn << "] out of sequence, sending status report " << endl;
            sendStatusReportLte();
        }

        if (received_.at(index) == true) {
            EV << NOW << " LteRlcAmRxEntity::enque the received PDU has index " << index << " which points to an already busy location" << endl;

            auto pktAux = check_and_cast<Packet *>(pduBuffer_.get(index));
            auto bufferedpdu = pktAux->peekAtFront<LteRlcAmPdu>();

            if (bufferedpdu->getSnoMainPacket() == pdu->getSnoMainPacket()) {
                EV << NOW << " LteRlcAmRxEntity::enque the received PDU with " << index << " was already buffered " << endl;
                delete pkt;
            }
            else {
                throw cRuntimeError("LteRlcAmRxEntity::enque(): the received PDU at position %d"
                                    "main SDU %d overlaps with an old one, main SDU %d", index, pdu->getSnoMainPacket(),
                        bufferedpdu->getSnoMainPacket());
            }
        }
        else {
            pduBuffer_.addAt(index, pkt);
            received_.at(index) = true;
            checkCompleteSdu(index);
        }
    }
}

void LteRlcAmRxEntity::passUpLte(const int index)
{
    Enter_Method("passUp");

    Packet *pkt = nullptr;

    auto header = check_and_cast<Packet *>(pduBuffer_.get(index))->peekAtFront<LteRlcAmPdu>();
    if (!header->isWhole()) {
        std::deque<Packet *> frameBuff;
        const auto pkId = header->getSnoMainPacket();

        if (index == 0 && !header->isFirst()) {
            for (auto& p : pendingPduBuffer_) {
                auto frgId = p->peekAtFront<LteRlcAmPdu>()->getSnoMainPacket();
                if (frgId != pkId) {
                    throw cRuntimeError("LteRlcAmRxEntity::passUp(): fragment buffer has fragments for SDU %d while trying to pass up %d", frgId, pkId);
                }
                frameBuff.push_back(p);
            }
            pendingPduBuffer_.clear();
        }

        int auxIndex = index;

        for (int i = 0; i < pduBuffer_.size() && frameBuff.size() < header->getTotalFragments(); i++) {
            auto headerAux = check_and_cast<Packet *>(pduBuffer_.get(auxIndex))->peekAtFront<LteRlcAmPdu>();
            if (pkId == headerAux->getSnoMainPacket())
                frameBuff.push_back(check_and_cast<Packet *>(pduBuffer_.get(auxIndex))->dup());
            auxIndex++;
            if (auxIndex >= pduBuffer_.size())
                auxIndex = 0;
        }

        pkt = defragmentFrames(frameBuff);
    }
    else {
        pkt = (check_and_cast<Packet *>(pduBuffer_.get(index)))->dup();
        pkt->removeAtFront<LteRlcAmPdu>();
    }

    pkt->trim();

    auto ci = pkt->getTag<FlowControlInfo>();

    Direction dir = ci->getDirection();
    MacNodeId dstId = ci->getDestId();
    MacNodeId srcId = ci->getSourceId();
    cModule *nodeb = nullptr;
    cModule *ue = nullptr;
    double delay = (NOW - pkt->getCreationTime()).dbl();

    if (dir == DL) {
        nodeb = binder_->getRlcByNodeId(srcId);
        ue = binder_->getRlcByNodeId(dstId);
    }
    else {
        nodeb = binder_->getRlcByNodeId(dstId);
        ue = binder_->getRlcByNodeId(srcId);
    }

    totalRcvdBytes_ += pkt->getByteLength();
    totalCellRcvdBytes_ += pkt->getByteLength();
    double tputSample = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    double cellTputSample = (double)totalCellRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());

    if (nodeb != nullptr) {
        nodeb->emit(rlcCellThroughputSignal_[dir_], cellTputSample);
        nodeb->emit(rlcCellPacketLossSignal_[dir_], 0.0);
    }
    if (ue != nullptr) {
        ue->emit(rlcThroughputSignal_[dir_], tputSample);
        ue->emit(rlcDelaySignal_[dir_], delay);
        ue->emit(rlcPacketLossSignal_[dir_], 0.0);
    }

    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::pdcp);
    send(pkt, "out");

    sendStatusReportLte();
}

void LteRlcAmRxEntity::checkCompleteSdu(const int index)
{
    auto pkt = check_and_cast<Packet *>(pduBuffer_.get(index));
    auto pdu = pkt->peekAtFront<LteRlcAmPdu>();

    int incomingSdu = pdu->getSnoMainPacket();

    EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu at position " << index << " for SDU number " << incomingSdu << endl;

    if (firstSdu_ == -1) {
        firstSdu_ = incomingSdu;
    }

    bool complete = false;
    bool bComplete = false;

    Ptr<LteRlcAmPdu> tempPdu = nullptr;
    int tempSdu = -1;
    int firstIndex = -1;
    if (pdu->isWhole()) {
        EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu - complete SDU has been found (PDU at " << index << " was whole)" << endl;
        passUpLte(index);
        return;
    }
    else {
        if (!pdu->isFirst()) {
            if ((index) == 0) {
                if (firstSdu_ == incomingSdu) {
                    firstIndex = index;
                    bComplete = true;
                }
                else
                    throw cRuntimeError("LteRlcAmRxEntity::checkCompleteSdu(): first SDU error : %d", firstSdu_);
            }
            else {
                for (int i = index - 1; i >= 0; i--) {
                    if (received_.at(i) == false) {
                        EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu: SDU cannot be reconstructed, no PDU received at positions earlier than " << i << endl;
                        return;
                    }
                    else {
                        auto tempPkt = check_and_cast<Packet *>(pduBuffer_.get(i));

                        tempPdu = constPtrCast<LteRlcAmPdu>(tempPkt->peekAtFront<LteRlcAmPdu>());
                        tempSdu = tempPdu->getSnoMainPacket();

                        if (tempSdu != incomingSdu)
                            throw cRuntimeError("LteRlcAmRxEntity::checkCompleteSdu(): backward search: fragmentation error: the receiver buffer contains parts of different SDUs, PDU seqnum %d", pdu->getSnoFragment());

                        if (tempPdu->isFirst()) {
                            firstIndex = i;
                            bComplete = true;
                            break;
                        }
                        else if (tempPdu->isLast() || tempPdu->isWhole()) {
                            auto auxPkt = check_and_cast<Packet *>(pduBuffer_.get(i + 1));
                            auto aux = auxPkt->peekAtFront<LteRlcAmPdu>();
                            throw cRuntimeError("LteRlcAmRxEntity::checkCompleteSdu(): backward search: sequence error, found last or whole PDU [%d] preceding a middle one [%d], belonging to SDU [%d], current SDU is [%d]", tempPdu->getSnoFragment(),
                                    aux->getSnoFragment(), aux->getSnoMainPacket(), tempSdu);
                        }
                    }
                }
            }
        }
        else {
            bComplete = true;
            firstIndex = index;
        }
    }
    if (!bComplete) {
        EV << NOW
           << " LteRlcAmRxEntity::checkCompleteSdu - SDU cannot be reconstructed, backward search didn't find any predecessors to PDU at "
           << index << endl;
        return;
    }
    if (pdu->isLast()) {
        EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu - complete SDU has been found, backward search was successful, and current was last of its SDU"
                     " passing up " << firstIndex << endl;
        passUpLte(firstIndex);
        return;
    }

    EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu initiating forward search, starting from position " << index + 1 << endl;

    for (int i = index + 1; i < (rxWindowDesc_.windowSize_); ++i) {
        if (received_.at(i) == false) {
            EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu forward search failed, no PDU at position " << i << " corresponding to"
                                                                                                           " SN  " << i + rxWindowDesc_.firstSeqNum_ << endl;
            return;
        }
        else {
            auto temPkt = check_and_cast<Packet *>(pduBuffer_.get(i));
            tempPdu = constPtrCast<LteRlcAmPdu>(temPkt->peekAtFront<LteRlcAmPdu>());
            tempSdu = tempPdu->getSnoMainPacket();
            if (tempSdu != incomingSdu)
                throw cRuntimeError("LteRlcAmRxEntity::checkCompleteSdu(): SDU numbers differ from position %d to %d : former SDU %d second %d", i, i - 1, incomingSdu, tempSdu);
        }
        if (tempPdu->isLast()) {
            complete = true;
            EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu: forward search successful, last PDU found at position "
               << i << endl;
            break;
        }
        else if (tempPdu->isFirst() || tempPdu->isWhole()) {
            throw cRuntimeError("LteRlcAmRxEntity::checkCompleteSdu(): forward search: PDU sequencer error ");
            break;
        }
    }

    if (complete) {
        EV << NOW << " LteRlcAmRxEntity::checkCompleteSdu - complete SDU has been found after forward search, passing up "
           << firstIndex << endl;
        passUpLte(firstIndex);
        return;
    }
}

void LteRlcAmRxEntity::sendStatusReportLte()
{
    Enter_Method("sendStatusReport()");
    EV << NOW << " LteRlcAmRxEntity::sendStatusReport " << endl;

    if ((NOW.dbl() - lastSentAck_.dbl()) < ackReportInterval_.dbl()) {
        EV << NOW << " LteRlcAmRxEntity::sendStatusReport , minimum interval not reached "
           << ackReportInterval_.dbl() << endl;
        return;
    }

    int highest = -1;
    for (int i = 0; i < rxWindowDesc_.windowSize_; ++i) {
        if (received_.at(i) == true)
            highest = i;
    }

    if (highest < 0) {
        EV << NOW << " LteRlcAmRxEntity::sendStatusReport : nothing received, no STATUS sent" << endl;
        return;
    }

    StatusPduData data;
    data.ackSn = rxWindowDesc_.firstSeqNum_ + highest + 1;
    for (int i = 0; i <= highest; ++i) {
        if (received_.at(i) == false) {
            NackInfo nack;
            nack.sn = rxWindowDesc_.firstSeqNum_ + i;
            data.nacks.push_back(nack);
        }
    }

    EV << NOW << " LteRlcAmRxEntity::sendStatusReport : ACK_SN " << data.ackSn
       << " with " << data.nacks.size() << " NACK(s)" << endl;

    auto pktPdu = new Packet("rlcAmPdu (STATUS)");
    auto pdu = makeShared<LteRlcAmPdu>();
    pdu->setAmType(ACK);
    pdu->setData(data);
    // LTE (TS 36.322) simplification: the STATUS PDU is modeled at the flat RLC_HEADER_AM
    // size and does not grow with the NACK count (unlike the NR path in sendStatusReportNr,
    // which sizes ACK_SN + per-NACK/NACK_range/SO fields exactly).
    pdu->setChunkLength(B(RLC_HEADER_AM));
    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *ackFlowControlInfo_;
    pktPdu->insertAtFront(pdu);
    bufferControlViaTxEntityLte(pktPdu);
    lastSentAck_ = NOW;
}

int LteRlcAmRxEntity::computeWindowShift() const
{
    EV << NOW << "LteRlcAmRxEntity::computeWindowShift" << endl;
    int shift = 0;
    for (int i = 0; i < rxWindowDesc_.windowSize_; ++i) {
        if (received_.at(i) == true || discarded_.at(i) == true) {
            ++shift;
        }
        else {
            break;
        }
    }
    return shift;
}

void LteRlcAmRxEntity::moveRxWindow(const int seqNum)
{
    EV << NOW << " LteRlcAmRxEntity::moveRxWindow moving forth to match first seqnum " << seqNum << endl;

    int pos = seqNum - rxWindowDesc_.firstSeqNum_;

    if (pos <= 0)
        return;

    if (pos > rxWindowDesc_.windowSize_)
        throw cRuntimeError("LteRlcAmRxEntity::moveRxWindow(): positions %d win size %d, seq num %d", pos, rxWindowDesc_.windowSize_, seqNum);

    int currentSdu = firstSdu_;

    EV << NOW << " LteRlcAmRxEntity::moveRxWindow current SDU is " << firstSdu_ << endl;

    for (int i = 0; i < pos; ++i) {
        if (pduBuffer_.get(i) != nullptr) {

            auto pktPdu = check_and_cast<Packet *>(pduBuffer_.remove(i));
            auto pdu = pktPdu->peekAtFront<LteRlcAmPdu>();
            currentSdu = (pdu->getSnoMainPacket());

            if (pdu->isLast() || pdu->isWhole()) {
                currentSdu = -1;
                for (auto& p : pendingPduBuffer_) {
                    delete p;
                }
                pendingPduBuffer_.clear();
                delete pktPdu;
            }
            else {
                pendingPduBuffer_.push_back(pktPdu);
            }
        }
        else {
            currentSdu = -1;
        }
    }

    for (int i = pos; i < rxWindowDesc_.windowSize_; ++i) {
        if (pduBuffer_.get(i) != nullptr) {
            pduBuffer_.addAt(i - pos, pduBuffer_.remove(i));
        }
        else {
            pduBuffer_.remove(i);
        }
        received_.at(i - pos) = received_.at(i);
        discarded_.at(i - pos) = discarded_.at(i);
        received_.at(i) = false;
        discarded_.at(i) = false;
    }

    rxWindowDesc_.firstSeqNum_ += pos;

    EV << NOW << " LteRlcAmRxEntity::moveRxWindow first sequence number updated to "
       << rxWindowDesc_.firstSeqNum_ << endl;

    firstSdu_ = currentSdu;
    EV << NOW << " LteRlcAmRxEntity::moveRxWindow current SDU updated to "
       << firstSdu_ << endl;
}

void LteRlcAmRxEntity::routeControlToTxEntityLte(Packet *pkt)
{
    // Received STATUS PDU: hand it to the co-located TX side of this AM entity
    // (feedbackOut is connected to tx.feedbackIn inside the RlcAmEntityBase compound)
    send(pkt, "feedbackOut");
}

void LteRlcAmRxEntity::bufferControlViaTxEntityLte(Packet *pkt)
{
    // Locally generated STATUS report: hand it to the co-located TX side for
    // transmission on this bearer's logical channel (statusOut -> tx.statusIn)
    send(pkt, "statusOut");
}

} //namespace
