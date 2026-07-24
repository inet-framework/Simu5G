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
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/NetworkInterface.h>

#include "simu5g/stack/rlc/am/RlcAmRxEntity.h"
#include "simu5g/stack/rlc/am/RlcAmTxEntity.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmDataPdu.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

namespace simu5g {

Define_Module(RlcAmRxEntity);

using namespace inet;
using namespace omnetpp;

unsigned int RlcAmRxEntity::totalCellRcvdBytes_ = 0;

// LTE statistics
simsignal_t RlcAmRxEntity::rlcCellPacketLossSignal_[2] = { registerSignal("rlcCellPacketLossDl"), registerSignal("rlcCellPacketLossUl") };
simsignal_t RlcAmRxEntity::rlcPacketLossSignal_[2] = { registerSignal("rlcPacketLossDl"), registerSignal("rlcPacketLossUl") };
simsignal_t RlcAmRxEntity::rlcPduPacketLossSignal_[2] = { registerSignal("rlcPduPacketLossDl"), registerSignal("rlcPduPacketLossUl") };
simsignal_t RlcAmRxEntity::rlcDelaySignal_[2] = { registerSignal("rlcDelayDl"), registerSignal("rlcDelayUl") };
simsignal_t RlcAmRxEntity::rlcThroughputSignal_[2] = { registerSignal("rlcThroughputDl"), registerSignal("rlcThroughputUl") };
simsignal_t RlcAmRxEntity::rlcPduDelaySignal_[2] = { registerSignal("rlcPduDelayDl"), registerSignal("rlcPduDelayUl") };
simsignal_t RlcAmRxEntity::rlcPduThroughputSignal_[2] = { registerSignal("rlcPduThroughputDl"), registerSignal("rlcPduThroughputUl") };
simsignal_t RlcAmRxEntity::rlcCellThroughputSignal_[2] = { registerSignal("rlcCellThroughputDl"), registerSignal("rlcCellThroughputUl") };

// NR statistics (declared only by the NrRlcAmRxEntity profile)
simsignal_t RlcAmRxEntity::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t RlcAmRxEntity::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");
simsignal_t RlcAmRxEntity::rxWindowOccupationSignal_ = registerSignal("rxWindowOccupation");

RlcAmRxEntity::RlcAmRxEntity() :
    lastSentAck_(0), timer_(this)
{
    timer_.setTimerId(BUFFERSTATUS_T);
}

RlcAmRxEntity::~RlcAmRxEntity()
{
    // LTE buffers (empty in NR mode)
    for (int i = 0; i < pduBuffer_.size(); i++) {
        if (pduBuffer_.get(i) != nullptr)
            delete check_and_cast<Packet *>(pduBuffer_.remove(i));
    }
    for (auto& p : pendingPduBuffer_)
        delete p;
    pendingPduBuffer_.clear();

    // NR buffers (null in LTE mode)
    delete rxBuffer_;
    if (tReassemblyTimer_)
        cancelAndDelete(tReassemblyTimer_);
    if (tStatusProhibitTimer_)
        cancelAndDelete(tStatusProhibitTimer_);

    delete ackFlowControlInfo_;
}

void RlcAmRxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        soFraming_ = par("soFraming");

        bearerManagement_ = check_and_cast<BearerManagement *>(
                inet::getContainingNicModule(this)->getSubmodule("rrc")->getSubmodule("bearerManagement"));

        if (soFraming_) {
            // The mux that feeds our "in" gate determines our stack (LTE vs NR).
            rlcMux_ = getModuleFromPar<RlcMux>(par("rlcMuxModule"), this);
            nameEntity_ = getFullPath();

            amWindowSize_ = par("AM_Window_Size");
            if (amWindowSize_ < 1 || (amWindowSize_ & (amWindowSize_ - 1)) != 0)
                throw cRuntimeError("RlcAmRxEntity::initialize() AM_Window_Size=%u must be a power of two (e.g. 512, 2048, 131072)", amWindowSize_);

            rxBuffer_ = new RlcSduSlidingWindowReceptionBuffer(amWindowSize_, nameEntity_ + "-rx-sliding window:");
            tReassemblyTimer_ = new cMessage("t_ReassemblyTimer");
            tReassembly_ = par("t_Reassembly");
            tStatusProhibitTimer_ = new cMessage("t_StatusProhibitTimer");
            tStatusProhibit_ = par("t_StatusProhibit");
        }
        else {
            rxWindowDesc_.windowSize_ = par("rxWindowSize");
            ackReportInterval_ = par("ackReportInterval");
            statusReportInterval_ = par("statusReportInterval");

            discarded_.resize(rxWindowDesc_.windowSize_);
            received_.resize(rxWindowDesc_.windowSize_);
            binder_.reference(this, "binderModule", true);

            LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
            dir_ = mac->getNodeType() == NODEB ? UL : DL;
        }
    }
}

void RlcAmRxEntity::handleMessage(cMessage *msg)
{
    if (soFraming_) {
        // --- NR ---
        if (msg->isSelfMessage()) {
            if (msg == tReassemblyTimer_) {
                rxBuffer_->handleReassemblyTimerExpiry(rxNextStatusTrigger_);
                bool hasHoles = rxBuffer_->hasMissingByteSegmentBeforeLast(rxBuffer_->getRxHighestStatus());
                bool restart = (rxBuffer_->getRxNextHighest() == rxBuffer_->getRxHighestStatus() && hasHoles);

                if (rxBuffer_->getRxNextHighest() > rxBuffer_->getRxHighestStatus() + 1 || restart) {
                    scheduleAfter(tReassembly_, tReassemblyTimer_);
                    rxNextStatusTrigger_ = rxBuffer_->getRxNextHighest();
                }
                sendStatusReportNr();
            }
            else if (msg == tStatusProhibitTimer_) {
                if (statusReportPending_)
                    sendStatusReportNr();
            }
            return;
        }

        auto pkt = check_and_cast<Packet *>(msg);
        auto chunk = pkt->peekAtFront<inet::Chunk>();
        if (inet::dynamicPtrCast<const NrRlcAmStatusPdu>(chunk) != nullptr) {
            routeControlToTxEntityNr(pkt);
        }
        else {
            emit(receivedPacketFromLowerLayerSignal_, pkt);
            enqueNr(pkt);
        }
        return;
    }

    // --- LTE ---
    if (msg->isSelfMessage()) {
        EV << NOW << "RlcAmRxEntity::handleMessage timer event received, sending status report " << endl;

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
            enqueLte(pkt);
        }
        else {
            throw cRuntimeError("RlcAmRxEntity: unexpected message from gate %s", incoming->getFullName());
        }
    }
}

void RlcAmRxEntity::enque(Packet *pkt)
{
    if (soFraming_) enqueNr(pkt);
    else enqueLte(pkt);
}

// ===================== LTE (fragment / SN-contiguity) implementation =====================

Packet *RlcAmRxEntity::defragmentFrames(std::deque<Packet *>& fragmentFrames)
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

void RlcAmRxEntity::discard(const int sn)
{
    int index = sn - rxWindowDesc_.firstSeqNum_;

    if ((index < 0) || (index >= rxWindowDesc_.windowSize_))
        throw cRuntimeError("RlcAmRxEntity::discard PDU %d out of rx window ", sn);

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
            throw cRuntimeError("RlcAmRxEntity::discard PDU at position %d already discarded", i);
    }

    EV << NOW << " RlcAmRxEntity::discard , discarded " << discarded << " PDUs " << endl;

    if (dir != UNKNOWN_DIRECTION) {
        cModule *ue = binder_->getRlcByNodeId((dir == DL ? dstId : srcId), UM);
        if (ue != nullptr)
            ue->emit(rlcPacketLossSignal_[dir_], 1.0);

        cModule *nodeb = binder_->getRlcByNodeId((dir == DL ? srcId : dstId), UM);
        if (nodeb != nullptr)
            nodeb->emit(rlcCellPacketLossSignal_[dir_], 1.0);
    }
}

void RlcAmRxEntity::enqueLte(Packet *pkt)
{
    auto pdu = pkt->peekAtFront<LteRlcAmPdu>();

    if (pdu->getAmType() != DATA) {
        if ((pdu->getAmType() == ACK)) {
            EV << NOW << " RlcAmRxEntity::enque Received ACK message" << endl;
            routeControlToTxEntityLte(pkt);
        }
        else {
            throw cRuntimeError("RLC AM - Unknown status PDU");
        }
        return;
    }

    if (timer_.idle()) {
        EV << NOW << " RlcAmRxEntity::enque reporting timer was idle, will fire at " << NOW.dbl() + statusReportInterval_.dbl() << endl;
        timer_.start(statusReportInterval_);
    }
    else {
        EV << NOW << " RlcAmRxEntity::enque reporting timer was already on, will fire at " << NOW.dbl() + timer_.remaining().dbl() << endl;
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
        EV << NOW << " RlcAmRxEntity::enque the received PDU with " << index << " was already buffered and discarded by MRW" << endl;
        delete pkt;
    }
    else if ((index >= rxWindowDesc_.windowSize_)) {
        throw cRuntimeError("RlcAmRxEntity::enque(): received PDU with position %d out of the window of size %d", index, rxWindowDesc_.windowSize_);
    }
    else {
        if (tsn == rxWindowDesc_.seqNum_) {
            rxWindowDesc_.seqNum_++;
            EV << NOW << "RlcAmRxEntity::enque DATA PDU received at index [" << index << "] with fragment number [" << tsn << "] in sequence " << endl;
        }
        else {
            rxWindowDesc_.seqNum_ = tsn + 1;
            EV << NOW << "RlcAmRxEntity::enque DATA PDU received at index [" << index << "] with fragment number ["
               << tsn << "] out of sequence, sending status report " << endl;
            sendStatusReportLte();
        }

        if (received_.at(index) == true) {
            EV << NOW << " RlcAmRxEntity::enque the received PDU has index " << index << " which points to an already busy location" << endl;

            auto pktAux = check_and_cast<Packet *>(pduBuffer_.get(index));
            auto bufferedpdu = pktAux->peekAtFront<LteRlcAmPdu>();

            if (bufferedpdu->getSnoMainPacket() == pdu->getSnoMainPacket()) {
                EV << NOW << " RlcAmRxEntity::enque the received PDU with " << index << " was already buffered " << endl;
                delete pkt;
            }
            else {
                throw cRuntimeError("RlcAmRxEntity::enque(): the received PDU at position %d"
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

void RlcAmRxEntity::passUpLte(const int index)
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
                    throw cRuntimeError("RlcAmRxEntity::passUp(): fragment buffer has fragments for SDU %d while trying to pass up %d", frgId, pkId);
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
        nodeb = binder_->getRlcByNodeId(srcId, UM);
        ue = binder_->getRlcByNodeId(dstId, UM);
    }
    else {
        nodeb = binder_->getRlcByNodeId(dstId, UM);
        ue = binder_->getRlcByNodeId(srcId, UM);
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

void RlcAmRxEntity::checkCompleteSdu(const int index)
{
    auto pkt = check_and_cast<Packet *>(pduBuffer_.get(index));
    auto pdu = pkt->peekAtFront<LteRlcAmPdu>();

    int incomingSdu = pdu->getSnoMainPacket();

    EV << NOW << " RlcAmRxEntity::checkCompleteSdu at position " << index << " for SDU number " << incomingSdu << endl;

    if (firstSdu_ == -1) {
        firstSdu_ = incomingSdu;
    }

    bool complete = false;
    bool bComplete = false;

    Ptr<LteRlcAmPdu> tempPdu = nullptr;
    int tempSdu = -1;
    int firstIndex = -1;
    if (pdu->isWhole()) {
        EV << NOW << " RlcAmRxEntity::checkCompleteSdu - complete SDU has been found (PDU at " << index << " was whole)" << endl;
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
                    throw cRuntimeError("RlcAmRxEntity::checkCompleteSdu(): first SDU error : %d", firstSdu_);
            }
            else {
                for (int i = index - 1; i >= 0; i--) {
                    if (received_.at(i) == false) {
                        EV << NOW << " RlcAmRxEntity::checkCompleteSdu: SDU cannot be reconstructed, no PDU received at positions earlier than " << i << endl;
                        return;
                    }
                    else {
                        auto tempPkt = check_and_cast<Packet *>(pduBuffer_.get(i));

                        tempPdu = constPtrCast<LteRlcAmPdu>(tempPkt->peekAtFront<LteRlcAmPdu>());
                        tempSdu = tempPdu->getSnoMainPacket();

                        if (tempSdu != incomingSdu)
                            throw cRuntimeError("RlcAmRxEntity::checkCompleteSdu(): backward search: fragmentation error: the receiver buffer contains parts of different SDUs, PDU seqnum %d", pdu->getSnoFragment());

                        if (tempPdu->isFirst()) {
                            firstIndex = i;
                            bComplete = true;
                            break;
                        }
                        else if (tempPdu->isLast() || tempPdu->isWhole()) {
                            auto auxPkt = check_and_cast<Packet *>(pduBuffer_.get(i + 1));
                            auto aux = auxPkt->peekAtFront<LteRlcAmPdu>();
                            throw cRuntimeError("RlcAmRxEntity::checkCompleteSdu(): backward search: sequence error, found last or whole PDU [%d] preceding a middle one [%d], belonging to SDU [%d], current SDU is [%d]", tempPdu->getSnoFragment(),
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
           << " RlcAmRxEntity::checkCompleteSdu - SDU cannot be reconstructed, backward search didn't find any predecessors to PDU at "
           << index << endl;
        return;
    }
    if (pdu->isLast()) {
        EV << NOW << " RlcAmRxEntity::checkCompleteSdu - complete SDU has been found, backward search was successful, and current was last of its SDU"
                     " passing up " << firstIndex << endl;
        passUpLte(firstIndex);
        return;
    }

    EV << NOW << " RlcAmRxEntity::checkCompleteSdu initiating forward search, starting from position " << index + 1 << endl;

    for (int i = index + 1; i < (rxWindowDesc_.windowSize_); ++i) {
        if (received_.at(i) == false) {
            EV << NOW << " RlcAmRxEntity::checkCompleteSdu forward search failed, no PDU at position " << i << " corresponding to"
                                                                                                           " SN  " << i + rxWindowDesc_.firstSeqNum_ << endl;
            return;
        }
        else {
            auto temPkt = check_and_cast<Packet *>(pduBuffer_.get(i));
            tempPdu = constPtrCast<LteRlcAmPdu>(temPkt->peekAtFront<LteRlcAmPdu>());
            tempSdu = tempPdu->getSnoMainPacket();
            if (tempSdu != incomingSdu)
                throw cRuntimeError("RlcAmRxEntity::checkCompleteSdu(): SDU numbers differ from position %d to %d : former SDU %d second %d", i, i - 1, incomingSdu, tempSdu);
        }
        if (tempPdu->isLast()) {
            complete = true;
            EV << NOW << " RlcAmRxEntity::checkCompleteSdu: forward search successful, last PDU found at position "
               << i << endl;
            break;
        }
        else if (tempPdu->isFirst() || tempPdu->isWhole()) {
            throw cRuntimeError("RlcAmRxEntity::checkCompleteSdu(): forward search: PDU sequencer error ");
            break;
        }
    }

    if (complete) {
        EV << NOW << " RlcAmRxEntity::checkCompleteSdu - complete SDU has been found after forward search, passing up "
           << firstIndex << endl;
        passUpLte(firstIndex);
        return;
    }
}

void RlcAmRxEntity::sendStatusReportLte()
{
    Enter_Method("sendStatusReport()");
    EV << NOW << " RlcAmRxEntity::sendStatusReport " << endl;

    if ((NOW.dbl() - lastSentAck_.dbl()) < ackReportInterval_.dbl()) {
        EV << NOW << " RlcAmRxEntity::sendStatusReport , minimum interval not reached "
           << ackReportInterval_.dbl() << endl;
        return;
    }

    int highest = -1;
    for (int i = 0; i < rxWindowDesc_.windowSize_; ++i) {
        if (received_.at(i) == true)
            highest = i;
    }

    if (highest < 0) {
        EV << NOW << " RlcAmRxEntity::sendStatusReport : nothing received, no STATUS sent" << endl;
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

    EV << NOW << " RlcAmRxEntity::sendStatusReport : ACK_SN " << data.ackSn
       << " with " << data.nacks.size() << " NACK(s)" << endl;

    auto pktPdu = new Packet("rlcAmPdu (STATUS)");
    auto pdu = makeShared<LteRlcAmPdu>();
    pdu->setAmType(ACK);
    pdu->setData(data);
    pdu->setChunkLength(B(RLC_HEADER_AM));
    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *ackFlowControlInfo_;
    pktPdu->insertAtFront(pdu);
    bufferControlViaTxEntityLte(pktPdu);
    lastSentAck_ = NOW;
}

int RlcAmRxEntity::computeWindowShift() const
{
    EV << NOW << "RlcAmRxEntity::computeWindowShift" << endl;
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

void RlcAmRxEntity::moveRxWindow(const int seqNum)
{
    EV << NOW << " RlcAmRxEntity::moveRxWindow moving forth to match first seqnum " << seqNum << endl;

    int pos = seqNum - rxWindowDesc_.firstSeqNum_;

    if (pos <= 0)
        return;

    if (pos > rxWindowDesc_.windowSize_)
        throw cRuntimeError("RlcAmRxEntity::moveRxWindow(): positions %d win size %d, seq num %d", pos, rxWindowDesc_.windowSize_, seqNum);

    int currentSdu = firstSdu_;

    EV << NOW << " RlcAmRxEntity::moveRxWindow current SDU is " << firstSdu_ << endl;

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

    EV << NOW << " RlcAmRxEntity::moveRxWindow first sequence number updated to "
       << rxWindowDesc_.firstSeqNum_ << endl;

    firstSdu_ = currentSdu;
    EV << NOW << " RlcAmRxEntity::moveRxWindow current SDU updated to "
       << firstSdu_ << endl;
}

void RlcAmRxEntity::routeControlToTxEntityLte(Packet *pkt)
{
    // Received STATUS PDU: hand it to the co-located TX side of this AM entity
    // (feedbackOut is connected to tx.feedbackIn inside the RlcAmEntityBase compound)
    send(pkt, "feedbackOut");
}

void RlcAmRxEntity::bufferControlViaTxEntityLte(Packet *pkt)
{
    // Locally generated STATUS report: hand it to the co-located TX side for
    // transmission on this bearer's logical channel (statusOut -> tx.statusIn)
    send(pkt, "statusOut");
}

// ===================== NR (SO byte-coverage) implementation =====================

void RlcAmRxEntity::enqueNr(Packet *pkt)
{
    Enter_Method("enque()");
    take(pkt);

    auto pdu = pkt->peekAtFront<NrRlcAmDataPdu>();

    if (ackFlowControlInfo_ == nullptr) {
        auto orig = pkt->getTag<FlowControlInfo>();
        ackFlowControlInfo_ = orig->dup();
        ackFlowControlInfo_->setSourceId(orig->getDestId());
        ackFlowControlInfo_->setDestId(orig->getSourceId());
        ackFlowControlInfo_->setDirection((orig->getDirection() == DL) ? UL : DL);
    }

    unsigned int sn = pdu->getPduSequenceNumber();

    if (!rxBuffer_->inWindow(sn)) {
        if (pdu->getPollStatus())
            sendStatusReportNr();
        delete pkt;
        return;
    }

    if (rxBuffer_->isReady(sn)) {
        if (pdu->getPollStatus())
            sendStatusReportNr();
        delete pkt;
        return;
    }

    int totalSduLength = pdu->getLengthMainPacket();
    int start = pdu->getStartOffset();
    int end = pdu->getEndOffset();

    auto segmentResult = rxBuffer_->handleSegment(sn, totalSduLength, start, end, pkt);
    if (segmentResult.second) {
        if (pdu->getPollStatus())
            sendStatusReportNr();
        delete pkt;
        return;
    }

    if (segmentResult.first) {
        passUpNr(sn);
        if (sn == rxBuffer_->getRxHighestStatus())
            rxBuffer_->updateRxHighestStatus();
        if (sn == rxBuffer_->getRxNext())
            rxBuffer_->updateRxNext();
    }

    if (pdu->getPollStatus()) {
        if (sn < rxBuffer_->getRxHighestStatus() || rxBuffer_->aboveWindow(sn))
            sendStatusReportNr();
    }

    unsigned int currentRxNext = rxBuffer_->getRxNext();
    bool hasHoles = rxBuffer_->hasMissingByteSegmentBeforeLast(currentRxNext);

    if (tReassemblyTimer_->isScheduled()) {
        bool noHolesAndStatus = (currentRxNext + 1 == rxNextStatusTrigger_ && !hasHoles);
        bool statusOff = (!rxBuffer_->inWindow(rxNextStatusTrigger_)
                && rxNextStatusTrigger_ != rxBuffer_->getRxNext() + amWindowSize_);

        if (currentRxNext == rxNextStatusTrigger_ || noHolesAndStatus || statusOff)
            cancelEvent(tReassemblyTimer_);
    }

    if (!tReassemblyTimer_->isScheduled()) {
        bool missingAndHole = (rxBuffer_->getRxNextHighest() == currentRxNext + 1 && hasHoles);
        if (rxBuffer_->getRxNextHighest() > currentRxNext + 1 || missingAndHole) {
            EV << NOW << " RlcAmRxEntity::enque() t_ReassemblyTimer scheduled" << endl;
            scheduleAfter(tReassembly_, tReassemblyTimer_);
            rxNextStatusTrigger_ = rxBuffer_->getRxNextHighest();
        }
    }
}

void RlcAmRxEntity::passUpNr(int seqNum)
{
    Enter_Method("passUp");

    Packet *bufferedPkt = rxBuffer_->consumeSdu(seqNum);
    if (!bufferedPkt)
        throw cRuntimeError("RlcAmRxEntity::passUp() null PDU for seqNum=%d", seqNum);

    auto pdu = bufferedPkt->removeAtFront<NrRlcAmDataPdu>();
    if (pdu->getNumSdu() < 1)
        throw cRuntimeError("RlcAmRxEntity::passUp() PDU has no SDU");

    size_t sduLengthPktLen;
    auto sdu = pdu->popSdu(sduLengthPktLen);
    auto sduPdcp = sdu->getTag<PdcpTrackingTag>();
    EV << NOW << " RlcAmRxEntity::passUp() SDU[" << sduPdcp->getPdcpSequenceNumber()
       << "] from PDU sn=" << seqNum << endl;

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

    totalRcvdBytes_ += sdu->getByteLength();
    double tput = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    emit(rlcCellThroughputSignal_[dir == DL ? 0 : 1], tput);

    emit(sentPacketToUpperLayerSignal_, sdu);
    send(sdu, "out");
    passedUpSdus_.insert(sduPdcp->getPdcpSequenceNumber());
    delete bufferedPkt;
}

void RlcAmRxEntity::sendStatusReportNr()
{
    Enter_Method("sendStatusReport()");

    if (tStatusProhibitTimer_->isScheduled()) {
        EV << NOW << " RlcAmRxEntity::sendStatusReport, minimum interval not reached "
           << tStatusProhibit_ << endl;
        statusReportPending_ = true;
        return;
    }

    StatusPduData data = rxBuffer_->generateStatusPduData();

    auto pktPdu = new Packet("NR STATUS AM PDU");
    auto pdu = makeShared<NrRlcAmStatusPdu>();
    pdu->setAmType(ACK);
    pdu->setData(data);

    unsigned int size = 3;
    for (const auto &nack : data.nacks) {
        size++;
        if (nack.isSegment)
            size += 4;
        else if (nack.nackRange > 1)
            size++;
    }
    pdu->setChunkLength(B(size));

    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *ackFlowControlInfo_;
    pktPdu->insertAtFront(pdu);

    EV << NOW << " RlcAmRxEntity::sendStatusReport. Last sent " << lastSentAck_ << endl;
    bufferControlViaTxEntityNr(pktPdu);
    lastSentAck_ = NOW;
    scheduleAfter(tStatusProhibit_, tStatusProhibitTimer_);
    statusReportPending_ = false;
}

void RlcAmRxEntity::routeControlToTxEntityNr(Packet *pkt)
{
    // Received STATUS PDU: hand it to the co-located TX side of this AM entity
    // (feedbackOut is connected to tx.feedbackIn inside the RlcAmEntity compound)
    send(pkt, "feedbackOut");
}

void RlcAmRxEntity::bufferControlViaTxEntityNr(Packet *pkt)
{
    // Locally generated STATUS report: hand it to the co-located TX side for
    // transmission on this bearer's logical channel (statusOut -> tx.statusIn)
    send(pkt, "statusOut");
}

} //namespace
