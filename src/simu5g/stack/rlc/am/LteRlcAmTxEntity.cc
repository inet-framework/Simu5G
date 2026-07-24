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

#include "simu5g/stack/rlc/am/LteRlcAmTxEntity.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"

namespace simu5g {

Define_Module(LteRlcAmTxEntity);

LteRlcAmTxEntity::LteRlcAmTxEntity() :
     pduTimer_(this), bufferStatusTimer_(this)
{
    pduTimer_.setTimerId(PDU_T);
    bufferStatusTimer_.setTimerId(BUFFER_T);
}

LteRlcAmTxEntity::~LteRlcAmTxEntity()
{
    while (!pduBuffer_.isEmpty())
        delete check_and_cast<Packet *>(pduBuffer_.pop());
    while (!sduQueue_.isEmpty())
        delete check_and_cast<Packet *>(sduQueue_.pop());
    for (int i = 0; i < pduRtxQueue_.size(); i++) {
        if (pduRtxQueue_.get(i) != nullptr)
            delete check_and_cast<Packet *>(pduRtxQueue_.remove(i));
    }
    delete currentSdu_;
}

void LteRlcAmTxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
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

void LteRlcAmTxEntity::handleMessage(cMessage *msg)
{
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
                    throw cRuntimeError("LteRlcAmTxEntity::handleMessage(): unexpected timer event received");
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

            enque(pkt);
        }
        else if (incoming->isName("macIn")) {
            auto pkt = check_and_cast<Packet *>(msg);
            auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
            unsigned int size = macSduRequest->getSduSize();
            sendPdus(size);
            delete pkt;
        }
        else if (incoming->isName("feedbackIn")) {
            // STATUS PDU received from the peer, handed over by the co-located RX side
            processControlPacket(check_and_cast<Packet *>(msg));
        }
        else if (incoming->isName("statusIn")) {
            // locally generated STATUS report from the co-located RX side
            bufferPduInternal(check_and_cast<Packet *>(msg));
        }
        else {
            throw cRuntimeError("LteRlcAmTxEntity: unexpected message from gate %s", incoming->getFullName());
        }
    }
}

void LteRlcAmTxEntity::enque(Packet *pkt)
{
    EV << NOW << " LteRlcAmTxEntity::enque - inserting new SDU  " << endl;

    sduQueue_.insert(pkt);

    if (currentSdu_ == nullptr) {
        if (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_ < txWindowDesc_.windowSize_) {
            addPdus();
        }
    }
}

std::deque<Packet *> *LteRlcAmTxEntity::fragmentFrame(Packet *frame, std::deque<int>& windowsIndex, RlcFragDesc rlcFragDesc)
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

void LteRlcAmTxEntity::addPdus()
{
    Enter_Method("addPdus()");

    unsigned int addedPdus = 0;

    while ((txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_) < txWindowDesc_.windowSize_) {
        if (currentSdu_ == nullptr && sduQueue_.isEmpty() && (fragmentList_ == nullptr || fragmentList_->size() == 0)) {
            EV << NOW << " LteRlcAmTxEntity::addPdus - No data to send " << endl;
            break;
        }

        if (currentSdu_ == nullptr) {
            EV << NOW << " LteRlcAmTxEntity::addPdus - No pending SDU has been found" << endl;
            auto pkt = check_and_cast<Packet *>(sduQueue_.pop());

            int nrFragments = ceil((double)pkt->getByteLength() / (double)fragDesc_.fragUnit_);

            if (txWindowDesc_.seqNum_ + nrFragments < txWindowDesc_.firstSeqNum_ + txWindowDesc_.windowSize_) {
                fragDesc_.startFragmentation(pkt->getByteLength(), txWindowDesc_.seqNum_);

                currentSdu_ = pkt;
                fragmentList_ = fragmentFrame(pkt->dup(), txWindowIndexList_, fragDesc_);

                EV << NOW << " LteRlcAmTxEntity::addPdus current SDU size "
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

        EV << NOW << " LteRlcAmTxEntity::addPdus - prepare new RLC PDU" << endl;

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
                throw cRuntimeError("LteRlcAmTxEntity::addPdus(): trying to add a PDU to a position marked received [%d] discarded [%d]",
                        (int)(received_.at(txWindowIndex)), (int)(discarded_.at(txWindowIndex)));
            }
        }
        else {
            delete pdu;
            throw cRuntimeError("LteRlcAmTxEntity::addPdus(): trying to add a PDU to a busy position [%d]", txWindowIndex);
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
    EV << NOW << " LteRlcAmTxEntity::addPdus - added " << addedPdus << " PDUs" << endl;
}

void LteRlcAmTxEntity::discard(const int seqNum)
{
    int txWindowIndex = seqNum - txWindowDesc_.firstSeqNum_;

    EV << NOW << " LteRlcAmTxEntity::discard sequence number [" << seqNum
       << "] window index [" << txWindowIndex << "]" << endl;

    if ((txWindowIndex < 0) || (txWindowIndex >= txWindowDesc_.windowSize_)) {
        throw cRuntimeError(" LteRlcAmTxEntity::discard(): requested to discard an out of window PDU :"
                            " sequence number %d , window first sequence is %d",
                seqNum, txWindowDesc_.firstSeqNum_);
    }

    if (discarded_.at(txWindowIndex) == true) {
        EV << " LteRlcAmTxEntity::discard requested to discard an already discarded PDU :"
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
            throw cRuntimeError("LteRlcAmTxEntity::discard(): trying to get access to missing PDU %d", i);

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

void LteRlcAmTxEntity::advanceTxWindow()
{
    EV << NOW << " LteRlcAmTxEntity::advanceTxWindow " << endl;

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
        EV << NOW << " LteRlcAmTxEntity::advanceTxWindow  shifting window to " << newFirstSn << endl;
        moveTxWindow(newFirstSn);
    }
}

void LteRlcAmTxEntity::moveTxWindow(const int seqNum)
{
    int pos = seqNum - txWindowDesc_.firstSeqNum_;

    if (pos <= 0)
        return;

    EV << NOW << " LteRlcAmTxEntity::moveTxWindow sequence number " << seqNum
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
            throw cRuntimeError("LteRlcAmTxEntity::moveTxWindow(): encountered empty PDU at location %d, shift position %d", i, pos);
    }

    for (int i = pos; i < (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); ++i) {
        if (pduRtxQueue_.get(i) != nullptr) {
            auto pdu = check_and_cast<Packet *>(pduRtxQueue_.remove(i));
            pduRtxQueue_.addAt(i - pos, pdu);
        }
        else {
            throw cRuntimeError("LteRlcAmTxEntity::moveTxWindow(): encountered empty PDU at location %d, shift position %d", i, pos);
        }

        received_.at(i - pos) = received_.at(i);
        discarded_.at(i - pos) = discarded_.at(i);

        received_.at(i) = false;
        discarded_.at(i) = false;
    }

    for (int i = (txWindowDesc_.seqNum_ - txWindowDesc_.firstSeqNum_); i < txWindowDesc_.windowSize_; ++i) {
        if (pduRtxQueue_.get(i) != nullptr)
            throw cRuntimeError("LteRlcAmTxEntity::moveTxWindow(): encountered busy PDU at location %d, shift position %d", i, pos);

        received_.at(i) = false;
        discarded_.at(i) = false;
    }

    txWindowDesc_.firstSeqNum_ += pos;

    EV << NOW << " LteRlcAmTxEntity::moveTxWindow completed. First sequence number "
       << txWindowDesc_.firstSeqNum_ << " current sequence number "
       << txWindowDesc_.seqNum_ << endl;

    addPdus();
}

void LteRlcAmTxEntity::bufferControlPdu(cPacket *pkt)
{
    bufferPdu(pkt);
}

void LteRlcAmTxEntity::bufferPdu(cPacket *pktAux)
{
    Enter_Method("bufferPdu()"); // Direct Method Call (from RX entity cross-module)
    take(pktAux);
    bufferPduInternal(check_and_cast<inet::Packet *>(pktAux));
}

void LteRlcAmTxEntity::bufferPduInternal(inet::Packet *pkt)
{
    EV << NOW << " LteRlcAmTxEntity : Enqueuing " << pkt->getName() << " of size "
       << pkt->getByteLength() << " for sending\n";

    bool needToTriggerMac = pduBuffer_.isEmpty();

    pduBuffer_.insert(pkt);

    if (needToTriggerMac) {
        sendNewDataNotificationLte(pkt);
    }
}

void LteRlcAmTxEntity::sendNewDataNotificationLte(inet::Packet *pkt)
{
    auto newData = new inet::Packet("AM-NewData");
    newData->copyTags(*pkt);
    newData->addTag<LteRlcNewDataTag>();
    send(newData, "out");
}

void LteRlcAmTxEntity::sendPdus(int size)
{
    auto pkt = pduBuffer_.front();
    if (pkt->getByteLength() <= size) {
        pkt = pduBuffer_.pop();
        EV << "LteRlcAmTxEntity::sendPdus sending a PDU of size "
           << pkt->getByteLength() << " (total requested: " << size << ")" << std::endl;
    }
    else {
        EV << NOW << " LteRlcAmTxEntity::sendPdus: Cannot send PDU - PDU is larger than requested size (size == "
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

void LteRlcAmTxEntity::handleControlPacket(cPacket *pkt)
{
    Enter_Method("handleControlPacket()");
    take(pkt);
    processControlPacket(check_and_cast<Packet *>(pkt));
}

void LteRlcAmTxEntity::processControlPacket(Packet *pktPdu)
{
    auto pdu = pktPdu->peekAtFront<LteRlcAmPdu>();

    short type = pdu->getAmType();

    switch (type) {
        case ACK: {
            const StatusPduData& data = pdu->getData();
            EV << NOW << " LteRlcAmTxEntity::handleControlPacket , received STATUS ACK_SN "
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

void LteRlcAmTxEntity::recvAck(const int seqNum)
{
    int index = seqNum - txWindowDesc_.firstSeqNum_;

    EV << NOW << " LteRlcAmTxEntity::recvAck ACK received for sequence number "
       << seqNum << " first sequence n. [" << txWindowDesc_.firstSeqNum_
       << "] index [" << index << "] " << endl;

    if (index < 0) {
        return;
    }

    if (index >= txWindowDesc_.windowSize_)
        throw cRuntimeError("LteRlcAmTxEntity::recvAck(): ACK greater than window size %d", txWindowDesc_.windowSize_);

    if (!(received_.at(index))) {
        if (pduTimer_.busy(index + txWindowDesc_.firstSeqNum_))
            pduTimer_.remove(index + txWindowDesc_.firstSeqNum_);
        received_.at(index) = true;
        ASSERT(pduRtxQueue_.get(index) != nullptr);
    }
}

void LteRlcAmTxEntity::recvCumulativeAck(const int seqNum)
{
    if ((seqNum < txWindowDesc_.firstSeqNum_) || (seqNum < 0)) {
        return;
    }
    else if ((unsigned int)seqNum > (txWindowDesc_.firstSeqNum_ + txWindowDesc_.windowSize_)) {
        throw cRuntimeError("LteRlcAmTxEntity::recvCumulativeAck(): SN %d exceeds window size %d", seqNum, txWindowDesc_.windowSize_);
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

void LteRlcAmTxEntity::pduTimerHandle(const int sn)
{
    Enter_Method("pduTimerHandle");

    int index = sn - txWindowDesc_.firstSeqNum_;

    EV << NOW << " LteRlcAmTxEntity::pduTimerHandle - sequence number " << sn << endl;

    pduTimer_.handle(sn);

    if ((index < 0) || (index >= txWindowDesc_.windowSize_))
        throw cRuntimeError("LteRlcAmTxEntity::pduTimerHandle(): The PDU [%d] for which the timer elapsed is out of the window: index [%d]", sn, index);

    if (pduRtxQueue_.get(index) == nullptr)
        throw cRuntimeError("LteRlcAmTxEntity::pduTimerHandle(): PDU %d not found", index);

    if (received_.at(index) == true)
        throw cRuntimeError(" LteRlcAmTxEntity::pduTimerHandle(): The PDU %d [index %d] has already been received", sn, index);

    auto pduPkt = check_and_cast<Packet *>(pduRtxQueue_.get(index));
    auto pdu = pduPkt->peekAtFront<LteRlcAmPdu>();

    int nextTxNumber = pdu->getTxNumber() + 1;

    if (nextTxNumber > maxRtx_) {
        EV << NOW << " LteRlcAmTxEntity::pduTimerHandle maximum transmissions reached; discard the PDU" << endl;
        discard(sn);
    }
    else {
        EV << NOW << " LteRlcAmTxEntity::pduTimerHandle starting new transmission" << endl;
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

unsigned int LteRlcAmTxEntity::getPendingDataVolume() const
{
    // Not exercised on the LTE path today (nothing calls it -- MAC BSR-style
    // reporting for LTE-AM is driven entirely by the "AM-NewData" notification
    // sequence, not by a pull on this getter). Provided as the natural LTE
    // analogue of the NR implementation so the shared abstract interface is
    // total: bytes already formed into PDUs, plus bytes still waiting to be
    // fragmented.
    return static_cast<unsigned int>(pduBuffer_.getByteLength() + sduQueue_.getByteLength());
}

} //namespace
