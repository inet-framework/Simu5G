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

#include <set>

#include <inet/common/ProtocolTag_m.h>

#include "simu5g/stack/rlc/am/AmTxQueue.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmDataPdu.h"
#include "simu5g/stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

namespace simu5g {

Define_Module(AmTxQueue);

simsignal_t AmTxQueue::wastedGrantedBytesSignal_ = registerSignal("wastedGrantedBytes");
simsignal_t AmTxQueue::enqueuedSduSizeSignal_ = registerSignal("enqueuedSduSize");
simsignal_t AmTxQueue::enqueuedSduRateSignal_ = registerSignal("enqueuedSduRate");
simsignal_t AmTxQueue::requestedPduSizeSignal_ = registerSignal("requestedPduSize");
simsignal_t AmTxQueue::txWindowOccupationSignal_ = registerSignal("txWindowOccupation");
simsignal_t AmTxQueue::txWindowFullSignal_ = registerSignal("txWindowFull");
simsignal_t AmTxQueue::retransmissionPduSignal_ = registerSignal("retransmissionPdu");
simsignal_t AmTxQueue::receivedPacketFromUpperLayerSignal_ = registerSignal("receivedPacketFromUpperLayer");
simsignal_t AmTxQueue::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");

AmTxQueue::AmTxQueue() :
     pduTimer_(this), bufferStatusTimer_(this)
{
    pduTimer_.setTimerId(PDU_T);
    bufferStatusTimer_.setTimerId(BUFFER_T);
}

AmTxQueue::~AmTxQueue()
{
    // LTE buffers (empty in NR mode)
    while (!pduBuffer_.isEmpty())
        delete check_and_cast<Packet *>(pduBuffer_.pop());
    while (!sduQueue_.isEmpty())
        delete check_and_cast<Packet *>(sduQueue_.pop());
    for (int i = 0; i < pduRtxQueue_.size(); i++) {
        if (pduRtxQueue_.get(i) != nullptr)
            delete check_and_cast<Packet *>(pduRtxQueue_.remove(i));
    }
    delete currentSdu_;

    // NR buffers (null/empty in LTE mode)
    if (tPollRetransmitTimer_)
        cancelAndDelete(tPollRetransmitTimer_);
    delete txBuffer_;
    delete rtxBuffer_;
    while (!sduBuffer_.empty()) {
        delete sduBuffer_.front();
        sduBuffer_.pop_front();
    }
    while (!controlBuffer_.empty()) {
        delete controlBuffer_.front();
        controlBuffer_.pop_front();
    }

    delete lteInfo_;
}

void AmTxQueue::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        soFraming_ = par("soFraming");

        if (soFraming_) {
            amWindowSize_ = par("AM_Window_Size");
            // The window is 2^(snBits-1), so it must be a power of two.
            if (amWindowSize_ < 1 || (amWindowSize_ & (amWindowSize_ - 1)) != 0)
                throw cRuntimeError("AmTxQueue::initialize() AM_Window_Size=%u must be a power of two (e.g. 512, 2048, 131072)", amWindowSize_);

            pollPdu_ = par("pollPDU");
            pollByte_ = par("pollByte");
            maxRtxThreshold_ = par("maxRtxThreshold");
            tPollRetransmit_ = par("t_PollRetransmit");
            tPollRetransmitTimer_ = new cMessage("t_PollRetransmit timer");

            nameEntity_ = getFullPath();
            txBuffer_ = new RlcSduSlidingWindowTransmissionBuffer(amWindowSize_, nameEntity_ + "-tx-sliding window:");
            rtxBuffer_ = new RlcSduRetransmissionBuffer(maxRtxThreshold_);
            lastSduSample_ = NOW;
        }
        else {
            maxRtx_ = par("maxRtx");
            fragDesc_.fragUnit_ = par("fragmentSize");
            pduRtxTimeout_ = par("pduRtxTimeout");
            ctrlPduRtxTimeout_ = par("ctrlPduRtxTimeout");
            bufferStatusTimeout_ = par("bufferStatusTimeout");
            txWindowDesc_.windowSize_ = par("txWindowSize");
            received_.resize(txWindowDesc_.windowSize_, false);
            discarded_.resize(txWindowDesc_.windowSize_ + 1, false);
        }
    }
}

void AmTxQueue::finish()
{
}

void AmTxQueue::handleMessage(cMessage *msg)
{
    if (soFraming_) {
        // --- NR ---
        if (msg->isSelfMessage()) {
            if (msg == tPollRetransmitTimer_) {
                pollPending_ = true;
                bool noPendingData = (txBuffer_->getTotalPendingBytes() == 0
                        && sduBuffer_.empty() && rtxBuffer_->getRetxPendingBytes() == 0);

                if (noPendingData || txBuffer_->windowFull()) {
                    uint32_t hsn = txBuffer_->getHighestSnTransmitted();
                    Packet *ptr = nullptr;
                    uint32_t totalLength = 0;
                    bool found = txBuffer_->getSduData(hsn, ptr, totalLength);
                    bool added = found && rtxBuffer_->addNack(hsn, true, 0, totalLength - 1);

                    if (!added) {
                        uint32_t next = txBuffer_->getTxNextAck();
                        while (txBuffer_->isInRtxRange(next)) {
                            if (!txBuffer_->isFullyAcknowledged(next)) {
                                if (txBuffer_->getSduData(next, ptr, totalLength)) {
                                    if (rtxBuffer_->addNack(next, true, 0, totalLength - 1))
                                        return;
                                }
                            }
                            ++next;
                        }
                    }
                }
            }
            return;
        }

        cGate *incoming = msg->getArrivalGate();
        if (incoming->isName("in")) {
            auto pkt = check_and_cast<Packet *>(msg);
            emit(receivedPacketFromUpperLayerSignal_, pkt);

            auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
            unsigned int sequenceNumber = pdcpHeader->getSequenceNumber();
            auto pdcpTag = pkt->addTagIfAbsent<PdcpTrackingTag>();
            pdcpTag->setPdcpSequenceNumber(sequenceNumber);
            pdcpTag->setOriginalPacketLength(pkt->getByteLength());

            enqueNr(pkt);
        }
        else if (incoming->isName("macIn")) {
            auto pkt = check_and_cast<Packet *>(msg);
            auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
            sendPdusNr(macSduRequest->getSduSize());
            delete pkt;
        }
        else if (incoming->isName("feedbackIn")) {
            // STATUS PDU received from the peer, handed over by the co-located RX side
            processControlPacketNr(check_and_cast<Packet *>(msg));
        }
        else if (incoming->isName("statusIn")) {
            // locally generated STATUS report from the co-located RX side
            bufferControlPduNrInternal(check_and_cast<Packet *>(msg));
        }
        else {
            throw cRuntimeError("AmTxQueue: unexpected message from gate %s", incoming->getFullName());
        }
        return;
    }

    // --- LTE ---
    if (msg->isSelfMessage()) {
        TTimerMsg *tmsg = check_and_cast<TTimerMsg *>(msg);
        RlcAmTimerType amType = static_cast<RlcAmTimerType>(tmsg->getTimerId());
        TTimerType type = static_cast<TTimerType>(tmsg->getType());

        switch (type) {
            case TTSIMPLE:
                // Periodic safety check: advance the window if its head is acked.
                advanceTxWindow();
                bufferStatusTimer_.handle();
                break;
            case TTMULTI:
                TMultiTimerMsg *tmtmsg = check_and_cast<TMultiTimerMsg *>(tmsg);
                if (amType == PDU_T)
                    pduTimerHandle(tmtmsg->getEvent());
                else
                    throw cRuntimeError("AmTxQueue::handleMessage(): unexpected timer event received");
                break;
        }
        delete tmsg;
    }
    else {
        cGate *incoming = msg->getArrivalGate();
        if (incoming->isName("in")) {
            auto pkt = check_and_cast<Packet *>(msg);

            auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
            unsigned int sequenceNumber = pdcpHeader->getSequenceNumber();
            auto pdcpTag = pkt->addTag<PdcpTrackingTag>();
            pdcpTag->setPdcpSequenceNumber(sequenceNumber);
            pdcpTag->setOriginalPacketLength(pkt->getByteLength());

            enqueLte(pkt);
        }
        else if (incoming->isName("macIn")) {
            auto pkt = check_and_cast<Packet *>(msg);
            auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
            unsigned int size = macSduRequest->getSduSize();
            sendPdusLte(size);
            delete pkt;
        }
        else if (incoming->isName("feedbackIn")) {
            // STATUS PDU received from the peer, handed over by the co-located RX side
            processControlPacketLte(check_and_cast<Packet *>(msg));
        }
        else if (incoming->isName("statusIn")) {
            // locally generated STATUS report from the co-located RX side; queue it
            // for transmission on this bearer's logical channel
            bufferPduInternal(check_and_cast<Packet *>(msg));
        }
        else {
            throw cRuntimeError("AmTxQueue: unexpected message from gate %s", incoming->getFullName());
        }
    }
}

// ===================== dispatchers =====================

void AmTxQueue::enque(Packet *sdu)
{
    if (soFraming_) enqueNr(sdu);
    else enqueLte(sdu);
}

void AmTxQueue::sendPdus(int size)
{
    if (soFraming_) sendPdusNr(size);
    else sendPdusLte(size);
}

void AmTxQueue::handleControlPacket(cPacket *pkt)
{
    if (soFraming_) handleControlPacketNr(pkt);
    else handleControlPacketLte(pkt);
}

void AmTxQueue::bufferControlPdu(cPacket *pkt)
{
    if (soFraming_) bufferControlPduNr(pkt);
    else bufferControlPduLte(pkt);
}

// ===================== LTE (fragment/whole-PDU ARQ) implementation =====================

void AmTxQueue::enqueLte(Packet *pkt)
{
    EV << NOW << " AmTxQueue::enque - inserting new SDU  " << endl;

    sduQueue_.insert(pkt);

    if (currentSdu_ == nullptr) {
        if (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_ < txWindowDesc_.windowSize_) {
            addPdus();
        }
    }
}

std::deque<Packet *> *AmTxQueue::fragmentFrame(Packet *frame, std::deque<int>& windowsIndex, RlcFragDesc rlcFragDesc)
{
    EV_DEBUG << "Fragmenting " << *frame << " into " << rlcFragDesc.totalFragments_ << " fragments.\n";
    B offset = B(0);
    std::deque<Packet *> *fragments = new std::deque<Packet *>();
    auto pdcpTag = frame->getTag<PdcpTrackingTag>();
    windowsIndex.clear();
    RlcWindowDesc tmp = txWindowDesc_;
    B fragUnit = B(rlcFragDesc.fragUnit_);

    for (size_t i = 0; i < rlcFragDesc.totalFragments_; i++) {
        std::string name = std::string(frame->getName()) + "-frag" + std::to_string(i);
        auto fragment = new Packet(name.c_str());
        B length = (i == rlcFragDesc.totalFragments_ - 1) ? B(frame->getTotalLength()) - offset : fragUnit;
        fragment->insertAtBack(frame->peekDataAt(offset, length));
        offset += length;
        auto pdu = makeShared<LteRlcAmPdu>();
        pdu->setAmType(DATA);
        pdu->setTotalFragments(rlcFragDesc.totalFragments_);
        pdu->setSnoFragment(tmp.seqNum_);
        pdu->setFirstSn(rlcFragDesc.firstSn_);
        pdu->setLastSn(rlcFragDesc.firstSn_ + rlcFragDesc.totalFragments_ - 1);
        pdu->setSnoMainPacket(pdcpTag->getPdcpSequenceNumber());
        pdu->setTxNumber(0);
        fragment->insertAtFront(pdu);
        EV_TRACE << "Created " << *fragment << " fragment.\n";
        int txWindowIndex = tmp.seqNum_ - tmp.firstSeqNum_;
        if (txWindowIndex >= 200)
            throw cRuntimeError("Illegal index");
        windowsIndex.push_back(txWindowIndex);
        fragment->copyTags(*frame);
        fragments->push_back(fragment);
        tmp.seqNum_++;
    }
    delete frame;
    EV_TRACE << "Created " << fragments->size() << " fragments.\n";
    return fragments;
}

void AmTxQueue::addPdus()
{
    Enter_Method("addPdus()");

    unsigned int addedPdus = 0;

    while ((txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_) < txWindowDesc_.windowSize_) {
        if (currentSdu_ == nullptr && sduQueue_.isEmpty() && (fragmentList_ == nullptr || fragmentList_->size() == 0)) {
            EV << NOW << " AmTxQueue::addPdus - No data to send " << endl;
            break;
        }

        if (currentSdu_ == nullptr) {
            EV << NOW << " AmTxQueue::addPdus - No pending SDU has been found" << endl;
            auto pkt = check_and_cast<Packet *>(sduQueue_.pop());

            int nrFragments = ceil((double)pkt->getByteLength() / (double)fragDesc_.fragUnit_);

            if (txWindowDesc_.seqNum_ + nrFragments < txWindowDesc_.firstSeqNum_ + txWindowDesc_.windowSize_) {
                fragDesc_.startFragmentation(pkt->getByteLength(), txWindowDesc_.seqNum_);

                currentSdu_ = pkt;
                fragmentList_ = fragmentFrame(pkt->dup(), txWindowIndexList_, fragDesc_);

                EV << NOW << " AmTxQueue::addPdus current SDU size "
                   << currentSdu_->getByteLength() << " will be fragmented in "
                   << fragDesc_.totalFragments_ << " PDUs, each  of size "
                   << fragDesc_.fragUnit_ << endl;

                if (lteInfo_ != nullptr)
                    delete lteInfo_;

                lteInfo_ = currentSdu_->getTag<FlowControlInfo>()->dup();
            }
        }

        if (fragmentList_ == nullptr) {
            break;
        }

        EV << NOW << " AmTxQueue::addPdus - prepare new RLC PDU" << endl;

        auto pdu = fragmentList_->front();
        fragmentList_->pop_front();
        auto txWindowIndex = txWindowIndexList_.front();
        txWindowIndexList_.pop_front();

        auto pduHeader = pdu->peekAtFront<LteRlcAmPdu>();

        if (pduHeader->getSnoFragment() != txWindowDesc_.seqNum_)
            throw cRuntimeError("PDU sequence numbers must be checked");

        if (pduRtxQueue_.get(txWindowIndex) == nullptr) {
            auto pduCopy = pdu->dup();
            pduRtxQueue_.addAt(txWindowIndex, pduCopy);

            if (txWindowIndex >= 200)
                throw cRuntimeError("Illegal index");

            if (received_.at(txWindowIndex) || discarded_.at(txWindowIndex)) {
                delete pdu;
                throw cRuntimeError("AmTxQueue::addPdus(): trying to add a PDU to a position marked received [%d] discarded [%d]",
                        (int)(received_.at(txWindowIndex)), (int)(discarded_.at(txWindowIndex)));
            }
        }
        else {
            delete pdu;
            throw cRuntimeError("AmTxQueue::addPdus(): trying to add a PDU to a busy position [%d]", txWindowIndex);
        }
        pduTimer_.add(pduRtxTimeout_, txWindowDesc_.seqNum_);

        if (fragDesc_.addFragment() || (fragmentList_ && fragmentList_->empty())) {
            delete fragmentList_;
            fragmentList_ = nullptr;
            txWindowIndexList_.clear();
            fragDesc_.resetFragmentation();
            delete currentSdu_;
            currentSdu_ = nullptr;
        }
        txWindowDesc_.seqNum_++;
        addedPdus++;

        if (bufferStatusTimer_.busy() == false) {
            bufferStatusTimer_.start(bufferStatusTimeout_);
        }

        bufferPdu(pdu);
    }
    ASSERT(fragmentList_ == nullptr);
    ASSERT(txWindowIndexList_.empty());
    EV << NOW << " AmTxQueue::addPdus - added " << addedPdus << " PDUs" << endl;
}

void AmTxQueue::discard(const int seqNum)
{
    int txWindowIndex = seqNum - txWindowDesc_.firstSeqNum_;

    EV << NOW << " AmTxQueue::discard sequence number [" << seqNum
       << "] window index [" << txWindowIndex << "]" << endl;

    if ((txWindowIndex < 0) || (txWindowIndex >= txWindowDesc_.windowSize_)) {
        throw cRuntimeError(" AmTxQueue::discard(): requested to discard an out of window PDU :"
                            " sequence number %d , window first sequence is %d",
                seqNum, txWindowDesc_.firstSeqNum_);
    }

    if (discarded_.at(txWindowIndex) == true) {
        EV << " AmTxQueue::discard requested to discard an already discarded PDU :"
              " sequence number" << seqNum << " , window first sequence is " << txWindowDesc_.firstSeqNum_ << endl;
    }
    else {
        discarded_.at(txWindowIndex) = true;
    }

    auto pkt = check_and_cast<Packet *>(pduRtxQueue_.get(txWindowIndex));
    auto pdu = pkt->peekAtFront<LteRlcAmPdu>();

    if (pduTimer_.busy(seqNum))
        pduTimer_.remove(seqNum);

    for (int i = (txWindowIndex + 1);
         i < (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); ++i)
    {
        if (pduRtxQueue_.get(i) != nullptr) {
            auto nextPdu = check_and_cast<Packet *>(pduRtxQueue_.get(i))->peekAtFront<LteRlcAmPdu>();
            if (pdu->getSnoMainPacket() == nextPdu->getSnoMainPacket()) {
                if (!discarded_.at(i)) {
                    discarded_.at(i) = true;
                    if (pduTimer_.busy(i + txWindowDesc_.firstSeqNum_))
                        pduTimer_.remove(i + txWindowDesc_.firstSeqNum_);
                }
            }
            else {
                break;
            }
        }
        else
            break;
    }
    for (int i = txWindowIndex - 1; i >= 0; i--) {
        if (pduRtxQueue_.get(i) == nullptr)
            throw cRuntimeError("AmTxBuffer::discard(): trying to get access to missing PDU %d", i);

        auto nextPdu = check_and_cast<Packet *>(pduRtxQueue_.get(i))->peekAtFront<LteRlcAmPdu>();

        if (pdu->getSnoMainPacket() == nextPdu->getSnoMainPacket()) {
            if (!discarded_.at(i)) {
                discarded_.at(i) = true;
            }
            if (pduTimer_.busy(i + txWindowDesc_.firstSeqNum_))
                pduTimer_.remove(i + txWindowDesc_.firstSeqNum_);
        }
        else {
            break;
        }
    }
    advanceTxWindow();
}

void AmTxQueue::advanceTxWindow()
{
    EV << NOW << " AmTxQueue::advanceTxWindow " << endl;

    int lastPdu = 0;
    bool toMove = false;

    for (int i = 0; i < (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); ++i) {
        if ((discarded_.at(i) == true) || (received_.at(i) == true)) {
            lastPdu = i;
            toMove = true;
        }
        else {
            break;
        }
    }

    if (toMove) {
        int newFirstSn = txWindowDesc_.firstSeqNum_ + lastPdu + 1;
        EV << NOW << " AmTxQueue::advanceTxWindow  shifting window to " << newFirstSn << endl;
        moveTxWindow(newFirstSn);
    }
}

void AmTxQueue::moveTxWindow(const int seqNum)
{
    int pos = seqNum - txWindowDesc_.firstSeqNum_;

    if (pos <= 0)
        return;

    EV << NOW << " AmTxQueue::moveTxWindow sequence number " << seqNum
       << " corresponding index " << pos << endl;

    for (int i = 0; i < pos; ++i) {
        if (pduRtxQueue_.get(i) != nullptr) {
            auto pdu = check_and_cast<Packet *>(pduRtxQueue_.remove(i));
            delete pdu;

            if (pduTimer_.busy(i + txWindowDesc_.firstSeqNum_)) {
                pduTimer_.remove(i + txWindowDesc_.firstSeqNum_);
            }
            received_.at(i) = false;
            discarded_.at(i) = false;
        }
        else
            throw cRuntimeError("AmTxQueue::moveTxWindow(): encountered empty PDU at location %d, shift position %d", i, pos);
    }

    for (int i = pos; i < (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); ++i) {
        if (pduRtxQueue_.get(i) != nullptr) {
            auto pdu = check_and_cast<Packet *>(pduRtxQueue_.remove(i));
            pduRtxQueue_.addAt(i - pos, pdu);
        }
        else {
            throw cRuntimeError("AmTxQueue::moveTxWindow(): encountered empty PDU at location %d, shift position %d", i, pos);
        }

        received_.at(i - pos) = received_.at(i);
        discarded_.at(i - pos) = discarded_.at(i);

        received_.at(i) = false;
        discarded_.at(i) = false;
    }

    for (int i = (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); i < txWindowDesc_.windowSize_; ++i) {
        if (pduRtxQueue_.get(i) != nullptr)
            throw cRuntimeError("AmTxQueue::moveTxWindow(): encountered busy PDU at location %d, shift position %d", i, pos);

        received_.at(i) = false;
        discarded_.at(i) = false;
    }

    txWindowDesc_.firstSeqNum_ += pos;

    EV << NOW << " AmTxQueue::moveTxWindow completed. First sequence number "
       << txWindowDesc_.firstSeqNum_ << " current sequence number "
       << txWindowDesc_.seqNum_ << endl;

    addPdus();
}

void AmTxQueue::bufferControlPduLte(cPacket *pkt)
{
    bufferPdu(pkt);
}

void AmTxQueue::bufferPdu(cPacket *pktAux)
{
    Enter_Method("bufferPdu()"); // Direct Method Call (from RX entity cross-module)
    take(pktAux);
    bufferPduInternal(check_and_cast<inet::Packet *>(pktAux));
}

void AmTxQueue::bufferPduInternal(inet::Packet *pkt)
{
    EV << NOW << " AmTxQueue : Enqueuing " << pkt->getName() << " of size "
       << pkt->getByteLength() << " for sending\n";

    bool needToTriggerMac = pduBuffer_.isEmpty();

    pduBuffer_.insert(pkt);

    if (needToTriggerMac) {
        sendNewDataNotificationLte(pkt);
    }
}

void AmTxQueue::sendNewDataNotificationLte(inet::Packet *pkt)
{
    auto newData = new inet::Packet("AM-NewData");
    newData->copyTags(*pkt);
    newData->addTag<LteRlcNewDataTag>();
    send(newData, "out");
}

void AmTxQueue::sendPdusLte(int size)
{
    auto pkt = pduBuffer_.front();
    if (pkt->getByteLength() <= size) {
        pkt = pduBuffer_.pop();
        EV << "AmTxQueue::sendPdus sending a PDU of size "
           << pkt->getByteLength() << " (total requested: " << size << ")" << std::endl;
    }
    else {
        EV << NOW << " AmTxQueue::sendPdus: Cannot send PDU - PDU is larger than requested size (size == "
           << size << endl;

        auto pktCopy = check_and_cast<Packet *>(pkt->dup());
        pktCopy->setName("lteRlcFragment (empty)");
        auto rlcPdu = pktCopy->removeAtFront<LteRlcAmPdu>();
        rlcPdu->markMutableIfExclusivelyOwned();
        rlcPdu->setChunkLength(inet::b(1));
        pktCopy->insertAtFront(rlcPdu);
        pkt = pktCopy;
    }

    auto inetPkt = check_and_cast<inet::Packet *>(pkt);
    inetPkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    send(inetPkt, "out");

    if (!pduBuffer_.isEmpty()) {
        sendNewDataNotificationLte(check_and_cast<inet::Packet *>(pduBuffer_.front()));
    }
}

void AmTxQueue::handleControlPacketLte(cPacket *pkt)
{
    Enter_Method("handleControlPacket()");
    take(pkt);
    processControlPacketLte(check_and_cast<Packet *>(pkt));
}

void AmTxQueue::processControlPacketLte(Packet *pktPdu)
{
    auto pdu = pktPdu->peekAtFront<LteRlcAmPdu>();

    short type = pdu->getAmType();

    switch (type) {
        case ACK: {
            const StatusPduData& data = pdu->getData();
            EV << NOW << " AmTxQueue::handleControlPacket , received STATUS ACK_SN "
               << data.ackSn << " with " << data.nacks.size() << " NACK(s)" << endl;

            std::set<unsigned int> nacked;
            for (const auto& nack : data.nacks)
                nacked.insert(nack.sn);

            for (int sn = txWindowDesc_.firstSeqNum_; sn < (int)data.ackSn; ++sn) {
                if (nacked.find(sn) == nacked.end())
                    recvAck(sn);
            }

            advanceTxWindow();
            break;
        }
    }

    ASSERT(pktPdu->getOwner() == this);
    delete pktPdu;
}

void AmTxQueue::recvAck(const int seqNum)
{
    int index = seqNum - txWindowDesc_.firstSeqNum_;

    EV << NOW << " AmTxBuffer::recvAck ACK received for sequence number "
       << seqNum << " first sequence n. [" << txWindowDesc_.firstSeqNum_
       << "] index [" << index << "] " << endl;

    if (index < 0) {
        return;
    }

    if (index >= txWindowDesc_.windowSize_)
        throw cRuntimeError("AmTxBuffer::recvAck(): ACK greater than window size %d", txWindowDesc_.windowSize_);

    if (!(received_.at(index))) {
        if (pduTimer_.busy(index + txWindowDesc_.firstSeqNum_))
            pduTimer_.remove(index + txWindowDesc_.firstSeqNum_);
        received_.at(index) = true;
        ASSERT(pduRtxQueue_.get(index) != nullptr);
    }
}

void AmTxQueue::recvCumulativeAck(const int seqNum)
{
    if ((seqNum < txWindowDesc_.firstSeqNum_) || (seqNum < 0)) {
        return;
    }
    else if ((unsigned int)seqNum > (txWindowDesc_.firstSeqNum_ + txWindowDesc_.windowSize_)) {
        throw cRuntimeError("AmTxQueue::recvCumulativeAck(): SN %d exceeds window size %d", seqNum, txWindowDesc_.windowSize_);
    }
    else {
        for (int i = 0; i <= (seqNum - txWindowDesc_.firstSeqNum_); ++i) {
            if (!(received_.at(i))) {
                if (pduTimer_.busy(i + txWindowDesc_.firstSeqNum_))
                    pduTimer_.remove(i + txWindowDesc_.firstSeqNum_);
                received_.at(i) = true;
            }
        }
    }
}

void AmTxQueue::pduTimerHandle(const int sn)
{
    Enter_Method("pduTimerHandle");

    int index = sn - txWindowDesc_.firstSeqNum_;

    EV << NOW << " AmTxQueue::pduTimerHandle - sequence number " << sn << endl;

    pduTimer_.handle(sn);

    if ((index < 0) || (index >= txWindowDesc_.windowSize_))
        throw cRuntimeError("AmTxQueue::pduTimerHandle(): The PDU [%d] for which the timer elapsed is out of the window: index [%d]", sn, index);

    if (pduRtxQueue_.get(index) == nullptr)
        throw cRuntimeError("AmTxQueue::pduTimerHandle(): PDU %d not found", index);

    if (received_.at(index) == true)
        throw cRuntimeError(" AmTxQueue::pduTimerHandle(): The PDU %d [index %d] has already been received", sn, index);

    auto pduPkt = check_and_cast<Packet *>(pduRtxQueue_.get(index));
    auto pdu = pduPkt->peekAtFront<LteRlcAmPdu>();

    int nextTxNumber = pdu->getTxNumber() + 1;

    if (nextTxNumber > maxRtx_) {
        EV << NOW << " AmTxQueue::pduTimerHandle maximum transmissions reached; discard the PDU" << endl;
        discard(sn);
    }
    else {
        EV << NOW << " AmTxQueue::pduTimerHandle starting new transmission" << endl;
        auto pduPkt = check_and_cast<Packet *>(pduRtxQueue_.remove(index));
        auto pduUpd = pduPkt->removeAtFront<LteRlcAmPdu>();
        pduUpd->markMutableIfExclusivelyOwned();

        pduUpd->setTxNumber(nextTxNumber);
        pduPkt->insertAtFront(pduUpd);
        pduRtxQueue_.add(pduPkt->dup());
        pduTimer_.add(pduRtxTimeout_, sn);
        bufferPdu(pduPkt);
    }
}

// ===================== NR (SO segmentation + polling) implementation =====================

void AmTxQueue::enqueNr(Packet *sdu)
{
    Enter_Method("AmTxQueue::enqueNr()");
    EV << NOW << " AmTxQueue::enque() - inserting new SDU " << sdu << endl;

    delete lteInfo_;
    lteInfo_ = sdu->getTag<FlowControlInfo>()->dup();

    take(sdu);
    auto *si = new SduInfo();
    si->sdu = sdu;
    sduBuffer_.push_back(si);
    ++receivedSdus_;

    sduSampleBytes_ += sdu->getByteLength();
    emit(enqueuedSduSizeSignal_, sdu->getByteLength());
    if ((NOW - lastSduSample_) >= 1) {
        emit(enqueuedSduRateSignal_, sduSampleBytes_ / (NOW - lastSduSample_));
        sduSampleBytes_ = 0;
        lastSduSample_ = NOW;
    }

    sendNewDataNotificationNr(sdu);
}

void AmTxQueue::sendPdusNr(int pduSize)
{
    Enter_Method("AmTxQueue::sendPdusNr()");
    EV << NOW << " AmTxQueue::sendPdus() - PDU with size " << pduSize << " requested from MAC" << endl;
    emit(requestedPduSizeSignal_, pduSize);

    if (radioLinkFailureDetected_) {
        EV << NOW << " " << nameEntity_ << " AmTxQueue::sendPdus() RLF detected, stopping" << endl;
        return;
    }

    int size = pduSize - RLC_HEADER_AM;

    if (size < 0) {
        auto pkt = new inet::Packet("lteRlcFragment (empty)");
        auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();
        rlcPdu->setChunkLength(inet::b(1));
        pkt->insertAtFront(rlcPdu);
        if (lteInfo_)
            *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
        sendPduToMac(pkt);
        return;
    }

    // TS 38.322: prioritize control PDUs
    if (!controlBuffer_.empty()) {
        auto *pktControl = check_and_cast<inet::Packet *>(controlBuffer_.front());
        controlBuffer_.pop_front();

        EV << NOW << " AmTxQueue::sendPdus() - sending Control PDU " << pktControl
           << " with size " << pktControl->getByteLength() << " bytes to lower layer" << endl;
        sendPduToMac(pktControl);

        unsigned int pendingData = getPendingDataVolume();
        if (pendingData > 0 && lteInfo_) {
            auto pkt = new inet::Packet("lteRlcFragment -Indicate new data");
            auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();
            rlcPdu->setChunkLength(B(pendingData));
            *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
            pkt->insertAtFront(rlcPdu);
            sendNewDataNotificationNr(pkt);
            delete pkt;
        }
        return;
    }

    if (sendRetransmission(pduSize)) {
        reportBufferStatus();
        return;
    }

    // Size the header from the front segment's continuation state before the carve, so it
    // matches what sendSegment will stamp (and what the scheduler reserved). An empty buffer
    // means a fresh SDU will be pulled below (start 0 -> first/complete, 2B).
    uint32_t st;
    unsigned int newHdr = txBuffer_->peekNextSegmentStart(st)
                        ? nrAmHeaderBytes((st > 0) ? NRUM_CONTINUATION : NRUM_FIRST)
                        : nrAmHeaderBytes(NRUM_FIRST);
    int newDataSize = pduSize - (int)newHdr;

    PendingSegment segment;
    segment.isValid = false;
    if (txBuffer_->getTotalPendingBytes() > 0)
        segment = txBuffer_->getSegmentForGrant(newDataSize);

    if (!segment.isValid) {
        if (sduBuffer_.empty()) {
            EV << NOW << " AmTxQueue::sendPdus() buffer empty, wasting grant" << endl;
            emit(wastedGrantedBytesSignal_, size);
            return;
        }

        SduInfo *si = sduBuffer_.front();
        auto *bufferedSdu = check_and_cast<inet::Packet *>(si->sdu);
        auto pdcpTag = bufferedSdu->getTag<PdcpTrackingTag>();
        int sduLength = pdcpTag->getOriginalPacketLength();

        if (txBuffer_->windowFull()) {
            emit(txWindowFullSignal_, 1);
            return;
        }

        txBuffer_->addSdu(sduLength, bufferedSdu);
        sduBuffer_.pop_front();
        segment = txBuffer_->getSegmentForGrant(newDataSize);
    }

    if (!segment.isValid) {
        EV << NOW << " AmTxQueue::sendPdus() no segment fits grant" << endl;
        emit(wastedGrantedBytesSignal_, size);
        return;
    }

    sendSegment(segment);
    emit(txWindowOccupationSignal_, txBuffer_->getTxNext() - txBuffer_->getTxNextAck());
    reportBufferStatus();
}

void AmTxQueue::sendSegment(PendingSegment segment)
{
    auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();

    uint32_t segmentSize = segment.end - segment.start;
    FramingInfo fi;  // 00 = full SDU
    if (segmentSize != segment.totalLength) {
        if (segment.start == 0) {
            fi.lastIsFragment = true;  // 01 = first segment
        }
        else if (segment.end == segment.totalLength - 1) {
            fi.firstIsFragment = true;  // 10 = last segment
        }
        else {
            fi.firstIsFragment = true;
            fi.lastIsFragment = true;  // 11 = middle segment
        }
    }

    auto *bufferedSdu = check_and_cast<inet::Packet *>(segment.ptr);
    auto pdcpTag = bufferedSdu->getTag<PdcpTrackingTag>();
    int sduLength = pdcpTag->getOriginalPacketLength();
    unsigned int pduSequenceNumber = segment.sn;
    sn_ = std::max(sn_, pduSequenceNumber);
    int sduSequenceNumber = pdcpTag->getPdcpSequenceNumber();

    rlcPdu->pushSdu(bufferedSdu->dup(), segmentSize);
    // TS 38.322 6.2.1.4 AM header: a non-first segment (start>0) carries the 16-bit SO (4B);
    // a complete SDU or first segment carries SI+SN only (2B). The carve budget above
    // reserved the same size, so the PDU exactly fills its requested grant slot.
    unsigned int hdr = nrAmHeaderBytes((segment.start > 0) ? NRUM_CONTINUATION : NRUM_FIRST);
    rlcPdu->setFramingInfo(fi);
    rlcPdu->setPduSequenceNumber(pduSequenceNumber);
    rlcPdu->setChunkLength(inet::B(hdr + segmentSize));
    rlcPdu->setSnoMainPacket(sduSequenceNumber);
    rlcPdu->setLengthMainPacket(sduLength);
    rlcPdu->setStartOffset(segment.start);
    rlcPdu->setEndOffset(segment.end);

    ++pduWithoutPoll_;
    byteWithoutPoll_ += segmentSize;
    rlcPdu->setPollStatus(checkPolling());

    std::string name = "NR AM RLC Fragment -" + std::to_string(pduSequenceNumber);
    auto pkt = new inet::Packet(name.c_str());
    pkt->insertAtFront(rlcPdu);
    if (lteInfo_)
        *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;

    sendPduToMac(pkt);
}

bool AmTxQueue::checkPolling()
{
    bool noPendingData = (txBuffer_->getTotalPendingBytes() == 0
            && sduBuffer_.empty() && rtxBuffer_->getRetxPendingBytes() == 0);

    if (pollPending_ || pduWithoutPoll_ > pollPdu_ || byteWithoutPoll_ >= pollByte_
            || noPendingData || txBuffer_->windowFull()) {
        pduWithoutPoll_ = 0;
        byteWithoutPoll_ = 0;
        pollSn_ = sn_;
        rescheduleAfter(tPollRetransmit_, tPollRetransmitTimer_);
        pollPending_ = false;
        return true;
    }
    return false;
}

void AmTxQueue::reportBufferStatus()
{
    unsigned int pendingData = getPendingDataVolume();
    if (pendingData > 0 && lteInfo_) {
        auto pkt = new inet::Packet("lteRlcFragment Inform MAC");
        auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();
        rlcPdu->setChunkLength(B(pendingData));
        *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
        pkt->insertAtFront(rlcPdu);
        sendNewDataNotificationNr(pkt);
        delete pkt;
    }
}

bool AmTxQueue::sendRetransmission(int pduSize)
{
    RetxTask next;
    if (!rtxBuffer_->getNextRetxTask(next))
        return false;

    uint32_t start = next.soStart;
    uint32_t end = next.soEnd;

    if (next.isWholeSdu) {
        Packet *ptr = nullptr;
        uint32_t totalLength = 0;
        if (txBuffer_->getSduData(next.sn, ptr, totalLength)) {
            start = 0;
            end = totalLength - 1;
        }
        else {
            if (radioLinkFailureDetected_)
                return false;
            throw cRuntimeError("AmTxQueue::sendRetransmission whole SDU sn=%u not found", next.sn);
        }
    }

    // TS 38.322 AM header from the finalized segment start; reserve it before re-carving.
    int hdr = (int)nrAmHeaderBytes((start > 0) ? NRUM_CONTINUATION : NRUM_FIRST);
    int budget = pduSize - hdr;
    if (budget < 0)
        return false;
    PendingSegment segment = txBuffer_->getRetransmissionSegment(next.sn, start, end, budget);
    if (!segment.isValid) {
        if (radioLinkFailureDetected_)
            return false;
        throw cRuntimeError("AmTxQueue::sendRetransmission SDU sn=%u: invalid segment", next.sn);
    }
    if (!segment.ptr) {
        if (radioLinkFailureDetected_)
            return false;
        throw cRuntimeError("AmTxQueue::sendRetransmission SDU sn=%u: null pointer", next.sn);
    }

    sendSegment(segment);
    rtxBuffer_->markRetransmitted(next);
    emit(retransmissionPduSignal_, 1);
    return true;
}

void AmTxQueue::handleControlPacketNr(cPacket *pkt)
{
    Enter_Method("handleControlPacket()");
    take(pkt);
    processControlPacketNr(check_and_cast<Packet *>(pkt));
}

void AmTxQueue::processControlPacketNr(Packet *pktPdu)
{
    auto pdu = pktPdu->peekAtFront<NrRlcAmStatusPdu>();
    StatusPduData data = pdu->getData();

    rtxBuffer_->beginStatusPduProcessing();
    std::set<uint32_t> nacks;
    bool restartPoll = false;

    for (size_t i = 0; i < data.nacks.size(); ++i) {
        const NackInfo &info = data.nacks[i];
        for (unsigned int j = 0; j < info.nackRange; ++j) {
            uint32_t nackedSn = info.sn + j;
            if (txBuffer_->isInRtxRange(nackedSn)) {
                bool isWhole = !info.isSegment;
                bool added = rtxBuffer_->addNack(nackedSn, isWhole, info.soStart, info.soEnd);
                if (!added) {
                    EV << nameEntity_ << " [CRITICAL] Radio Link Failure" << endl;
                    radioLinkFailureDetected_ = true;
                    delete pktPdu;
                    return;
                }
            }
            nacks.insert(nackedSn);
            if (nackedSn == pollSn_)
                restartPoll = true;
        }
    }

    uint32_t next = txBuffer_->getTxNextAck();
    while (next < data.ackSn) {
        if (nacks.find(next) == nacks.end()) {
            uint32_t totalLength;
            Packet *sdu = nullptr;
            if (txBuffer_->getSduData(next, sdu, totalLength)) {
                std::set<uint32_t> acked = txBuffer_->handleAck(next, 0, totalLength - 1, pollSn_, restartPoll);
                for (uint32_t ackedSn : acked)
                    rtxBuffer_->clearSdu(ackedSn);
            }
        }
        ++next;
    }

    if (tPollRetransmitTimer_->isScheduled() && restartPoll)
        rescheduleAfter(tPollRetransmit_, tPollRetransmitTimer_);

    emit(txWindowOccupationSignal_, txBuffer_->getCurrentWindowSize());
    delete pktPdu;
}

void AmTxQueue::bufferControlPduNr(cPacket *pkt)
{
    Enter_Method("AmTxQueue::bufferControlPdu()");
    take(pkt);
    bufferControlPduNrInternal(check_and_cast<inet::Packet *>(pkt));
}

void AmTxQueue::bufferControlPduNrInternal(inet::Packet *pkt)
{
    controlBuffer_.push_back(pkt);
    sendNewDataNotificationNr(pkt);
}

void AmTxQueue::sendPduToMac(inet::Packet *pkt)
{
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    emit(sentPacketToLowerLayerSignal_, pkt);
    send(pkt, "out");
}

void AmTxQueue::sendNewDataNotificationNr(inet::Packet *pkt)
{
    auto newData = new inet::Packet("AM-NewData");
    newData->copyTags(*pkt);
    if (!newData->findTag<PdcpTrackingTag>()) {
        auto t = newData->addTag<PdcpTrackingTag>();
        t->setOriginalPacketLength(pkt->getByteLength());
        t->setPdcpSequenceNumber(0);
    }
    newData->addTag<LteRlcNewDataTag>();
    send(newData, "out");
}

unsigned int AmTxQueue::getPendingDataVolume() const
{
    unsigned int size = 0;
    for (const auto *si : sduBuffer_) {
        auto *pkt = check_and_cast<inet::Packet *>(si->sdu);
        auto pdcpTag = pkt->getTag<PdcpTrackingTag>();
        size += pdcpTag->getOriginalPacketLength();
    }
    size += txBuffer_->getTotalPendingBytes();
    size += rtxBuffer_->getRetxPendingBytes();
    for (const auto *cpkt : controlBuffer_) {
        auto *p = check_and_cast<const Packet *>(cpkt);
        auto statusPdu = p->peekAtFront<NrRlcAmStatusPdu>();
        size += statusPdu->getChunkLength().get();
    }
    return size;
}

} //namespace
