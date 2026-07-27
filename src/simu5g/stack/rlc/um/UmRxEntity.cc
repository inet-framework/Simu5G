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

#include <inet/networklayer/common/NetworkInterface.h>
#include "simu5g/stack/rlc/um/UmRxEntity.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/mec/utils/MecCommon.h"

namespace simu5g {

Define_Module(UmRxEntity);

using namespace inet;

unsigned int UmRxEntity::totalCellPduRcvdBytes_ = 0;
unsigned int UmRxEntity::totalCellRcvdBytes_ = 0;

// LTE statistics (throughput and delay; emitted on the per-UE/eNB RLC module)
simsignal_t UmRxEntity::rlcDelaySignal_[2] = { cComponent::registerSignal("rlcDelayDl"), cComponent::registerSignal("rlcDelayUl") };
simsignal_t UmRxEntity::rlcThroughputSignal_[2] = { cComponent::registerSignal("rlcThroughputDl"), cComponent::registerSignal("rlcThroughputUl") };
simsignal_t UmRxEntity::rlcPduDelaySignal_[2] = { cComponent::registerSignal("rlcPduDelayDl"), cComponent::registerSignal("rlcPduDelayUl") };
simsignal_t UmRxEntity::rlcPduThroughputSignal_[2] = { cComponent::registerSignal("rlcPduThroughputDl"), cComponent::registerSignal("rlcPduThroughputUl") };
simsignal_t UmRxEntity::rlcCellThroughputSignal_[2] = { cComponent::registerSignal("rlcCellThroughputDl"), cComponent::registerSignal("rlcCellThroughputUl") };
simsignal_t UmRxEntity::rlcDelayD2DSignal_ = registerSignal("rlcDelayD2D");
simsignal_t UmRxEntity::rlcThroughputD2DSignal_ = registerSignal("rlcThroughputD2D");
simsignal_t UmRxEntity::rlcPduDelayD2DSignal_ = registerSignal("rlcPduDelayD2D");
simsignal_t UmRxEntity::rlcPduThroughputD2DSignal_ = registerSignal("rlcPduThroughputD2D");

// NR statistics (emitted on this entity; declared only by the NrUmRxEntity profile)
simsignal_t UmRxEntity::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t UmRxEntity::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");

UmRxEntity::UmRxEntity() :
    t_reordering_(this)
{
    t_reordering_.setTimerId(REORDERING_T);
    buffered_.pkt = nullptr;
    buffered_.size = 0;
}

UmRxEntity::~UmRxEntity()
{
    Enter_Method("~UmRxEntity");
    delete buffered_.pkt;
    if (t_ReassemblyTimer)
        cancelAndDelete(t_ReassemblyTimer);
    if (sduBuffer) {
        sduBuffer->clearBuffer();
        delete sduBuffer;
    }
}

/*
 * Main functions
 */

void UmRxEntity::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        soFraming_ = par("soFraming");

        LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
        ownerNodeId_ = mac->getMacNodeId();

        if (soFraming_) {
            UM_Window_Size = par("UM_Window_Size");
            // Window = 2^(snBits-1). 3GPP UM SN lengths: LTE 5/10 bits (window 16/512),
            // NR 6/12 bits (window 32/2048).
            int snBits;
            switch (UM_Window_Size) {
                case 16:   snBits = 5;  break;
                case 32:   snBits = 6;  break;
                case 512:  snBits = 10; break;
                case 2048: snBits = 12; break;
                default:
                    throw cRuntimeError("UmRxEntity::initialize() UM_Window_Size=%d, but only 16, 32, 512 or 2048 are valid", UM_Window_Size);
            }
            sduBuffer = new RlcUmReceptionBuffer(snBits);
            t_ReassemblyTimer = new cMessage("UM Reassembly timer");
            t_Reassembly = par("t_Reassembly");
            // The mux feeding our "in" gate (for UL burst-throughput reporting).
            rlcMux_ = getModuleFromPar<RlcMux>(par("rlcMuxModule"), this);
        }
        else {
            timeout_ = par("timeout").doubleValue();
            rxWindowDesc_.clear();
            rxWindowDesc_.windowSize_ = par("rxWindowSize");
            received_.resize(rxWindowDesc_.windowSize_);
            rlcMux_ = getModuleFromPar<RlcMux>(par("rlcMuxModule"), this);
            dir_ = mac->getNodeType() == NODEB ? UL : DL;
            WATCH(timeout_);
        }
    }
    else if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS) {
        if (!soFraming_) {
            binder_.reference(this, "binderModule", true);
            LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
            nodeB_ = binder_->getRlcByNodeId(mac->getMacCellId(), UM);
            // ASSERT(nodeB_ != nullptr); -- see commit message why this is commented out
        }
    }
}

void UmRxEntity::handleMessage(cMessage *msg)
{
    if (soFraming_) {
        if (msg == t_ReassemblyTimer) {
            sduBuffer->onTimerExpiry();
            if (sduBuffer->startTimer())
                scheduleAfter(t_Reassembly, t_ReassemblyTimer);
            return;
        }
        auto pkt = check_and_cast<Packet *>(msg);
        if (pkt->getArrivalGate() && pkt->getArrivalGate()->isName("in")) {
            emit(receivedPacketFromLowerLayerSignal_, pkt);
            enqueNr(pkt);
        }
        else {
            throw cRuntimeError("UmRxEntity: unexpected message %s", msg->getFullName());
        }
        return;
    }

    // LTE mode
    if (msg->isSelfMessage()) {
        t_reordering_.handle();

        EV << NOW << " UmRxEntity::handleMessage : t_reordering timer has expired " << endl;

        unsigned int old = rxWindowDesc_.firstSnoForReordering_;

        // move to the first missing SN
        while (received_.at(rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_) == true
               || rxWindowDesc_.firstSnoForReordering_ < rxWindowDesc_.reorderingSno_)
        {
            rxWindowDesc_.firstSnoForReordering_++;
            if (rxWindowDesc_.firstSnoForReordering_ == rxWindowDesc_.highestReceivedSno_) // end of the window
                break;
        }

        int index = old - rxWindowDesc_.firstSno_;
        for (unsigned int i = index; i < rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_; i++) {
            // try to reassemble
            reassemble(i);
        }

        if (rxWindowDesc_.highestReceivedSno_ > rxWindowDesc_.firstSnoForReordering_) {
            rxWindowDesc_.reorderingSno_ = rxWindowDesc_.highestReceivedSno_;
            t_reordering_.start(timeout_);
        }

        delete msg;
    }
    else if (msg->getArrivalGate() && msg->getArrivalGate()->isName("in")) {
        // RLC PDU from LowerMux — enqueue for reassembly
        auto pkt = check_and_cast<inet::Packet *>(msg);
        EV << "UmRxEntity::handleMessage - Enqueue packet " << pkt->getName() << " from LowerMux\n";
        enqueLte(pkt);
    }
    else {
        throw cRuntimeError("UmRxEntity: unexpected message %s", msg->getFullName());
    }
}

void UmRxEntity::enque(cPacket *pkt)
{
    if (soFraming_)
        enqueNr(check_and_cast<inet::Packet *>(pkt));
    else
        enqueLte(pkt);
}

// ===================== LTE (FI/concatenation) implementation =====================

void UmRxEntity::enqueLte(cPacket *pktAux)
{
    Enter_Method("enque()");
    take(pktAux);

    EV << NOW << " UmRxEntity::enque - buffering new PDU" << endl;

    auto pktPdu = check_and_cast<Packet *>(pktAux);
    auto pdu = pktPdu->peekAtFront<LteRlcUmDataPdu>();
    auto lteInfo = pktPdu->getTag<FlowControlInfo>();

    // Get the RLC PDU Transmission sequence number (x)
    unsigned int tsn = pdu->getPduSequenceNumber();

    if (!init_ && isD2DMultiConnection()) {
        // for D2D multicast connections, the first received PDU must be considered as the first valid PDU
        rxWindowDesc_.clear(tsn);
        // setting the window size to 1 allows the entity to deliver immediately out-of-sequence SDU,
        // since reordering is not applicable for D2D multicast communications
        rxWindowDesc_.windowSize_ = 1;
        init_ = true;
    }

    // get the position in the buffer
    int index = tsn - rxWindowDesc_.firstSno_;

    EV << NOW << " UmRxEntity::enque - tsn " << tsn << ", the corresponding index in the buffer is " << index << endl;

    // x was already received
    if (tsn >= rxWindowDesc_.firstSnoForReordering_ && tsn < rxWindowDesc_.highestReceivedSno_ && received_.at(index) == true) {
        EV << NOW << " UmRxEntity::enque the received PDU has index " << index << " which points to an already busy location. Discard the PDU" << endl;
        delete pktPdu;
        return;
    }

    // x was already considered for reordering & reassembling
    if (tsn < rxWindowDesc_.firstSnoForReordering_) {
        EV << NOW << " UmRxEntity::enque the received PDU with " << tsn << " SN was already considered for reordering. Discard the PDU" << endl;
        delete pktPdu;
        return;
    }

    // x falls outside the rxWindow
    if (tsn >= rxWindowDesc_.highestReceivedSno_) {
        // move forward the rxWindow and try to reassemble

        unsigned int old = rxWindowDesc_.highestReceivedSno_;
        rxWindowDesc_.highestReceivedSno_ = tsn + 1;
        if (rxWindowDesc_.firstSno_ + rxWindowDesc_.windowSize_ < rxWindowDesc_.highestReceivedSno_) {
            int shift = rxWindowDesc_.highestReceivedSno_ - old;
            while (shift > 0) {

                // if "shift" is greater than the window size, we advance the window in several steps

                int p = (shift < rxWindowDesc_.windowSize_) ? shift : rxWindowDesc_.windowSize_;
                shift -= p;
                if (rxWindowDesc_.firstSno_ + p > tsn) { // HACK to avoid that the window goes ahead of the received tsn
                    p = tsn - rxWindowDesc_.firstSno_;
                }

                for (int i = 0; i < p; i++) {
                    // try to reassemble the PDU
                    reassemble(i);
                }

                // move the window (update buffer and firstSno)
                moveRxWindow(p);
            }

            // check whether firstSnoForReordering_ falls out of the window
            if (rxWindowDesc_.firstSnoForReordering_ < rxWindowDesc_.firstSno_) {
                rxWindowDesc_.firstSnoForReordering_ = rxWindowDesc_.firstSno_;
            }
        }
    }

    // buffer the received PDU at the correct position in the buffer
    // get the position in the buffer (the buffer may have been shifted)
    index = tsn - rxWindowDesc_.firstSno_;
    pduBuffer_.addAt(index, pktPdu);
    received_.at(index) = true;

    // emit statistics
    MacNodeId ueId;
    if (lteInfo->getDirection() == DL || lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI)                                                                                                                    // This module is at a UE
        ueId = ownerNodeId_;
    else           // UL. This module is at the eNB: get the node id of the sender
        ueId = lteInfo->getSourceId();

    double tputSample = (double)totalPduRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());

    // emit statistics
    cModule *ue = binder_->getRlcByNodeId(ueId, UM);
    if (ue != nullptr) {
        if (lteInfo->getDirection() != D2D && lteInfo->getDirection() != D2D_MULTI) { // UE in IM
            ue->emit(rlcPduThroughputSignal_[dir_], tputSample);
            ue->emit(rlcPduDelaySignal_[dir_], (NOW - pktPdu->getCreationTime()).dbl());
        }
        else { // UE in DM
            ue->emit(rlcPduThroughputD2DSignal_, tputSample);
            ue->emit(rlcPduDelayD2DSignal_, (NOW - pktPdu->getCreationTime()).dbl());
        }
    }

    EV << NOW << " UmRxEntity::enque - tsn " << tsn << ", the corresponding index after shift in the buffer is " << index << endl;
    EV << NOW << " UmRxEntity::enque - firstSnoReordering " << rxWindowDesc_.firstSnoForReordering_ << endl;

    index = rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_; //

    // D
    if (received_.at(rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_) == true) {
        unsigned int old = rxWindowDesc_.firstSnoForReordering_;

        index = rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_; //

        // move to the first missing SN
        while (received_.at(rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_) == true) {
            rxWindowDesc_.firstSnoForReordering_++;
            if (rxWindowDesc_.firstSnoForReordering_ == rxWindowDesc_.highestReceivedSno_) // end of the window
                break;
        }

        int index = old - rxWindowDesc_.firstSno_;
        for (unsigned int i = index; i < rxWindowDesc_.firstSnoForReordering_ - rxWindowDesc_.firstSno_; i++) {
            // try to reassemble
            reassemble(i);
        }
    }

    // handle t-reordering

    // if t_reordering is running
    if (t_reordering_.busy()) {
        if (rxWindowDesc_.reorderingSno_ <= rxWindowDesc_.firstSnoForReordering_ ||
            rxWindowDesc_.reorderingSno_ < rxWindowDesc_.firstSno_ || rxWindowDesc_.reorderingSno_ > rxWindowDesc_.highestReceivedSno_)
        {
            t_reordering_.stop();
        }
    }
    // if t_reordering is not running
    if (!t_reordering_.busy()) {
        if (rxWindowDesc_.highestReceivedSno_ > rxWindowDesc_.firstSnoForReordering_) {
            t_reordering_.start(timeout_);
            rxWindowDesc_.reorderingSno_ = rxWindowDesc_.highestReceivedSno_;
        }
    }

    if (flowControlInfo_->getDirection() == UL) { //only eNodeB checks the burst
        handleBurst(ENQUE);
    }
}

void UmRxEntity::moveRxWindow(int pos)
{
    EV << NOW << " UmRxEntity::moveRxWindow moving forth by " << pos << " locations" << endl;

    if (pos <= 0)
        return;                   // ignore the shift, it is ineffective.

    if (pos > rxWindowDesc_.windowSize_)
        throw cRuntimeError("AmRxQueue::moveRxWindow(): positions %d win size %d ", pos, rxWindowDesc_.windowSize_);

    for (unsigned int i = pos; i < rxWindowDesc_.windowSize_; ++i) {
        if (pduBuffer_.get(i) != nullptr) {
            pduBuffer_.addAt(i - pos, pduBuffer_.remove(i));
        }
        else {
            pduBuffer_.remove(i);
        }
        received_.at(i - pos) = received_.at(i);
        received_.at(i) = false;
    }

    rxWindowDesc_.firstSno_ += pos;

    EV << NOW << " UmRxEntity::moveRxWindow first sequence number updated to " << rxWindowDesc_.firstSno_ << endl;
}

void UmRxEntity::toPdcpLte(Packet *pktAux)
{
    // Remove leftover tag added on the TX side.
    pktAux->removeTagIfPresent<PdcpTrackingTag>();

    auto lteInfo = pktAux->getTag<FlowControlInfo>();
    unsigned int length = pktAux->getByteLength();
    simtime_t ts = pktAux->getCreationTime();

    // create a PDCP PDU and send it to the upper layer
    MacNodeId ueId;
    if (lteInfo->getDirection() == DL || lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI)
        ueId = ownerNodeId_;
    else           // UL. This module is at the eNB: get the node id of the sender
        ueId = lteInfo->getSourceId();

    cModule *ue = binder_->getRlcByNodeId(ueId, UM);

    // emit statistics (throughput and delay only - no packet loss)
    totalCellRcvdBytes_ += length;
    totalRcvdBytes_ += length;
    double cellTputSample = (double)totalCellRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    double tputSample = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());

    if (ue != nullptr) {
        if (lteInfo->getDirection() != D2D && lteInfo->getDirection() != D2D_MULTI) { // UE in IM
            ue->emit(rlcThroughputSignal_[dir_], tputSample);
            ue->emit(rlcDelaySignal_[dir_], (NOW - ts).dbl());
        }
        else {
            ue->emit(rlcThroughputD2DSignal_, tputSample);
            ue->emit(rlcDelayD2DSignal_, (NOW - ts).dbl());
        }
    }

    if (nodeB_ == nullptr) {
        // retry getting nodeB_, if it failed in initialize() due to cellId=0 in MAC (some race condition?)
        LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
        nodeB_ = binder_->getRlcByNodeId(mac->getMacCellId(), UM);
        ASSERT(nodeB_ != nullptr);
    }

    if (nodeB_ != nullptr) {
        nodeB_->emit(rlcCellThroughputSignal_[dir_], cellTputSample);
    }

    EV << NOW << " UmRxEntity::toPdcp Created PDCP PDU with length " << pktAux->getByteLength() << " bytes" << endl;
    EV << NOW << " UmRxEntity::toPdcp Send packet to upper layer" << endl;

    send(pktAux, "out");
}

void UmRxEntity::reassemble(unsigned int index)
{
    Enter_Method("reassemble()");

    if (received_.at(index) == false) {
        // consider the case when a PDU is missing or already delivered
        EV << NOW << " UmRxEntity::reassemble PDU at index " << index << " has not been received or already delivered" << endl;
        return;
    }

    EV << NOW << " UmRxEntity::reassemble Consider PDU at index " << index << " for reassembly" << endl;

    auto pktPdu = check_and_cast<Packet *>(pduBuffer_.get(index));
    auto pdu = pktPdu->removeAtFront<LteRlcUmDataPdu>();
    auto lteInfo = pktPdu->getTag<FlowControlInfo>();

    // get PDU seq number
    unsigned int pduSno = pdu->getPduSequenceNumber();

    if (resetFlag_) {
        // by doing this, the arrived PDU will be considered in order. For example, when D2D is enabled,
        // this helps to retrieve the synchronization between SNs at the tx and rx after a mode switch
        lastPduReassembled_ = pduSno - 1;
    }

    // get framing info
    FramingInfo fi = pdu->getFramingInfo();

    // get the number of (portions of) SDUs in the PDU
    unsigned int numSdu = pdu->getNumSdu();

    // for each SDU
    for (unsigned int i = 0; i < numSdu; i++) {
        size_t sduLengthPktLeng;
        auto pktSdu = check_and_cast<Packet *>(pdu->popSdu(sduLengthPktLeng));

        *pktSdu->addTag<FlowControlInfo>() = *flowControlInfo_;

        auto pdcpTag = pktSdu->getTag<PdcpTrackingTag>();
        unsigned int sduSno = pdcpTag->getPdcpSequenceNumber();
        unsigned int sduWholeLength = pdcpTag->getOriginalPacketLength(); // the length of the whole sdu

        if (i == 0) { // first SDU
            bool ignoreFragment = false;
            if (resetFlag_) {
                // by doing this, the first extracted SDU will be considered in order.
                resetFlag_ = false;
                ignoreFragment = true;
            }

            if (i == numSdu - 1) { // [first SDU, i==0] there is only one SDU in this PDU
                // read the FI field
                switch (fi.toValue()) {
                    case 0: {  // FI=00
                        EV << NOW << " UmRxEntity::reassemble The PDU includes one whole SDU [sno=" << sduSno << "]" << endl;
                        if (sduLengthPktLeng != sduWholeLength)
                            throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %zu B, while the original SDU had size %d B", sduLengthPktLeng, sduWholeLength);

                        toPdcpLte(pktSdu);
                        pktSdu = nullptr;

                        clearBufferedSdu();

                        break;
                    }
                    case 1: {  // FI=01
                        EV << NOW << " UmRxEntity::reassemble The PDU includes the first part [" << sduLengthPktLeng << " B] of an SDU [sno=" << sduSno << "]" << endl;

                        clearBufferedSdu();

                        // buffer the SDU and wait for the missing portion
                        buffered_.pkt = pktSdu;
                        pktSdu = nullptr;
                        buffered_.size = sduLengthPktLeng;
                        buffered_.currentPduSno = pduSno;

                        // for burst
                        ttiBits_ += sduLengthPktLeng;
                        EV << NOW << " UmRxEntity::reassemble Wait for the missing part..." << endl;

                        break;
                    }
                    case 2: {  // FI=10
                        // it is the last portion of an SDU, take the awaiting SDU
                        EV << NOW << " UmRxEntity::reassemble The PDU includes the last part [" << sduLengthPktLeng << " B] of an SDU [sno=" << sduSno << "]" << endl;

                        // check SDU SN
                        if (buffered_.pkt == nullptr ||
                            (pdcpTag->getPdcpSequenceNumber() != buffered_.pkt->getTag<PdcpTrackingTag>()->getPdcpSequenceNumber()) ||
                            (pduSno != (buffered_.currentPduSno + 1)) ||
                            ignoreFragment)
                        {
                            if (pduSno != buffered_.currentPduSno) {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, Pdu sequence numbers are not in sequence" << endl;
                            }
                            else {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, first part missing" << endl;
                            }
                            clearBufferedSdu();

                            delete pktSdu;
                            pktSdu = nullptr;
                            continue;
                        }

                        EV << NOW << " UmRxEntity::reassemble The waiting SDU has size " << buffered_.size << " bytes" << endl;

                        unsigned int reassembledLength = buffered_.size + sduLengthPktLeng;
                        if (reassembledLength < sduWholeLength) {
                            clearBufferedSdu();
                            EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, mid part missing" << endl;

                            delete pktSdu;
                            pktSdu = nullptr;
                            continue;
                        }
                        else if (reassembledLength > sduWholeLength) {
                            throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %d B, while the original SDU had size %d B", reassembledLength, sduWholeLength);
                        }

                        // for burst
                        ttiBits_ += sduLengthPktLeng;
                        toPdcpLte(pktSdu);
                        pktSdu = nullptr;

                        clearBufferedSdu();

                        break;
                    }
                    case 3: {  // FI=11
                        // add the length of this SDU to the awaiting SDU and wait for the missing portion
                        EV << NOW << " UmRxEntity::reassemble The PDU includes the mid part [" << sduLengthPktLeng << " B] of an SDU [sno=" << sduSno << "]" << endl;

                        // check SDU SN
                        if (buffered_.pkt == nullptr ||
                            (pdcpTag->getPdcpSequenceNumber() != buffered_.pkt->getTag<PdcpTrackingTag>()->getPdcpSequenceNumber()) ||
                            (pduSno != (buffered_.currentPduSno + 1)) ||
                            ignoreFragment)
                        {
                            if (pduSno != buffered_.currentPduSno) {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, Pdu sequence numbers are not in sequence" << endl;
                            }
                            else {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, first part missing" << endl;
                            }
                            clearBufferedSdu();

                            delete pktSdu;
                            pktSdu = nullptr;
                            continue;
                        }

                        // for burst
                        ttiBits_ += sduLengthPktLeng;
                        buffered_.size += sduLengthPktLeng;
                        buffered_.currentPduSno = pduSno;
                        delete pktSdu;
                        pktSdu = nullptr;

                        EV << NOW << " UmRxEntity::reassemble The waiting SDU has size " << buffered_.size << " bytes, was " << buffered_.size - sduLengthPktLeng << " bytes" << endl;
                        EV << NOW << " UmRxEntity::reassemble Wait for the missing part..." << endl;

                        break;
                    }
                    default: {
                        throw cRuntimeError("UmRxEntity::reassemble(): FI field was not valid %d ", fi.toValue());
                    }
                }
            }
            else { // [first SDU, i==0] there is more than one SDU in this PDU
                EV << NOW << " UmRxEntity::reassemble Read the first chunk of the PDU" << endl;

                // read the FI field
                if (!fi.firstIsFragment) {
                    {  // FI=00 or FI=01
                        // it is a whole SDU, send the SDU to the PDCP
                        EV << NOW << " UmRxEntity::reassemble This is a whole SDU [sno=" << sduSno << "]" << endl;
                        if (sduLengthPktLeng != sduWholeLength)
                            throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %zu B, while the original SDU had size %d B", sduLengthPktLeng, sduWholeLength);

                        // for burst
                        ttiBits_ += sduLengthPktLeng;
                        toPdcpLte(pktSdu);
                        pktSdu = nullptr;

                        clearBufferedSdu();
                    }
                }
                else {
                    {  // FI=10 or FI=11
                        // it is the last portion of an SDU, take the awaiting SDU and send to the PDCP
                        EV << NOW << " UmRxEntity::reassemble This is the last part [" << sduLengthPktLeng << " B] of an SDU [sno=" << sduSno << "]" << endl;

                        // check SDU SN
                        if (buffered_.pkt == nullptr ||
                            (pdcpTag->getPdcpSequenceNumber() != buffered_.pkt->getTag<PdcpTrackingTag>()->getPdcpSequenceNumber()) ||
                            (pduSno != (buffered_.currentPduSno + 1)) ||
                            ignoreFragment)
                        {
                            if (pduSno != (buffered_.currentPduSno + 1)) {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, Pdu sequence numbers are not in sequence" << endl;
                            }
                            else {
                                EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, first part missing" << endl;
                            }
                            clearBufferedSdu();
                            EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, first part missing" << endl;

                            delete pktSdu;
                            pktSdu = nullptr;

                            continue;
                        }

                        EV << NOW << " UmRxEntity::reassemble The waiting SDU has size " << buffered_.size << " bytes" << endl;

                        unsigned int reassembledLength = buffered_.size + sduLengthPktLeng;
                        if (reassembledLength < sduWholeLength) {
                            clearBufferedSdu();
                            EV << NOW << " UmRxEntity::reassemble The SDU cannot be reassembled, mid part missing" << endl;

                            delete pktSdu;

                            continue;
                        }
                        else if (reassembledLength > sduWholeLength) {
                            throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %d B, while the original SDU had size %d B", reassembledLength, sduWholeLength);
                        }

                        // for burst
                        ttiBits_ += sduWholeLength; // remove the discarded SDU size from the throughput
                        toPdcpLte(pktSdu);
                        pktSdu = nullptr;

                        clearBufferedSdu();
                    }
                }
            }
        }
        else if (i == numSdu - 1) { // last SDU in PDU with at least 2 SDUs
            // read the FI field
            if (!fi.lastIsFragment) {
                {  // FI=00 or FI=10
                    // it is a whole SDU, send the SDU to the PDCP
                    EV << NOW << " UmRxEntity::reassemble This is a whole SDU [sno=" << sduSno << "]" << endl;
                    if (sduLengthPktLeng != sduWholeLength)
                        throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %zu B, while the original SDU had size %d B", sduLengthPktLeng, sduWholeLength);

                    // for burst
                    ttiBits_ += sduLengthPktLeng;

                    toPdcpLte(pktSdu);
                    pktSdu = nullptr;

                    clearBufferedSdu();
                }
            }
            else {
                {  // FI=01 or FI=11
                    // it is the first portion of an SDU, buffer it
                    EV << NOW << " UmRxEntity::reassemble The PDU includes the first part [" << sduLengthPktLeng << " B] of an SDU [sno=" << sduSno << "]" << endl;

                    clearBufferedSdu();

                    // for burst
                    ttiBits_ += sduLengthPktLeng;

                    buffered_.pkt = pktSdu;
                    buffered_.size = sduLengthPktLeng;
                    buffered_.currentPduSno = pduSno;
                    pktSdu = nullptr;

                    EV << NOW << " UmRxEntity::reassemble Wait for the missing part..." << endl;
                }
            }
        }
        else {
            // it is a whole SDU, send to the PDCP
            EV << NOW << " UmRxEntity::reassemble This is a whole SDU [sno=" << sduSno << "]" << endl;
            if (sduLengthPktLeng != sduWholeLength)
                throw cRuntimeError("UmRxEntity::reassemble(): failed reassembly, the reassembled SDU has size %zu B, while the original SDU had size %d B", sduLengthPktLeng, sduWholeLength);

            // for burst
            ttiBits_ += sduLengthPktLeng;
            toPdcpLte(pktSdu);
            pktSdu = nullptr;

            clearBufferedSdu();
        }

        if (pktSdu != nullptr) {
            delete pktSdu;
            pktSdu = nullptr;
        }
    }
    // remove PDU from buffer
    pduBuffer_.remove(index);
    received_.at(index) = false;
    EV << NOW << " UmRxEntity::reassemble Removed PDU from position " << index << endl;

    // update the last PDU reassembled to the current PDU sequence number
    lastPduReassembled_ = pduSno;

    pktPdu->insertAtFront(pdu);

    delete pktPdu;
}

void UmRxEntity::clearBufferedSdu()
{
    if (buffered_.pkt != nullptr) {
        // for burst
        ttiBits_ -= buffered_.size; // remove the discarded SDU size from the throughput
        delete buffered_.pkt;
        buffered_.pkt = nullptr;
        buffered_.size = 0;
        buffered_.currentPduSno = 0;
    }
}

void UmRxEntity::rlcHandleD2DModeSwitchLte(bool oldConnection, bool oldMode, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == UE && oldMode == IM) {
            EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {

            EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - clear RX buffer of the RLC entity associated with the old mode" << endl;
            for (unsigned int i = 0; i < rxWindowDesc_.windowSize_; i++) {
                // try to reassemble
                reassemble(i);
            }

            // clear the buffer
            pduBuffer_.clear();

            for (auto && i : received_) {
                i = false;
            }

            clearBufferedSdu();

            // stop the timer
            if (t_reordering_.busy())
                t_reordering_.stop();
        }
    }
    else {
        EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - handle numbering of the RLC entity associated with the newly selected mode" << endl;

        // reset sequence numbering
        rxWindowDesc_.clear();

        resetFlag_ = true;
    }
}

// ===================== NR (SO byte-offset) implementation =====================

void UmRxEntity::enqueNr(Packet *pktPdu)
{
    EV << NOW << " UmRxEntity::enque - buffering new PDU" << endl;

    auto pdu = pktPdu->removeAtFront<NrRlcUmDataPdu>();

    // A full SDU (FI=00) can be delivered immediately; a segment goes to the
    // reassembly buffer.
    FramingInfo fi = pdu->getFramingInfo();
    if (fi.toValue() == 0) {
        size_t sduLength;
        auto pktSdu = check_and_cast<Packet *>(pdu->popSdu(sduLength));
        ttiBits_ += sduLength;   // UL burst accounting
        toPdcpNr(pktSdu);
    }
    else {
        unsigned int tsn = pdu->getPduSequenceNumber();
        ttiBits_ += (pdu->getEndOffset() - pdu->getStartOffset() + 1);   // UL burst accounting
        handlePDUInReceivedBuffer(pdu, tsn);
    }

    pktPdu->insertAtFront(pdu);
    delete pktPdu;

    // UL data-burst throughput accounting is done only at the eNB on the UL leg
    if (flowControlInfo_ && flowControlInfo_->getDirection() == UL)
        handleBurst(ENQUE);
}

void UmRxEntity::handlePDUInReceivedBuffer(inet::Ptr<NrRlcUmDataPdu> pdu, unsigned int tsn)
{
    size_t totalLength = pdu->getLengthMainPacket();
    auto sdu = check_and_cast<Packet *>(pdu->popSdu(totalLength));

    bool complete = sduBuffer->handleSegment(tsn, totalLength, pdu->getStartOffset(), pdu->getEndOffset(), sdu);
    if (complete) // otherwise the SDU is buffered or was discarded
        toPdcpNr(sdu);

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

void UmRxEntity::toPdcpNr(Packet *pktAux)
{
    // Set the receive-side flow info for PDCP routing and drop the TX-internal tag.
    *pktAux->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;
    pktAux->removeTagIfPresent<PdcpTrackingTag>();

    Direction dir = static_cast<Direction>(flowControlInfo_->getDirection());

    // Cell throughput at the RLC layer (emitted on this entity)
    totalRcvdBytesNr_ += pktAux->getByteLength();
    double tput = (double)totalRcvdBytesNr_ / (NOW - getSimulation()->getWarmupPeriod());
    emit(rlcCellThroughputSignal_[dir == UL ? 1 : 0], tput);

    emit(sentPacketToUpperLayerSignal_, pktAux);

    EV << NOW << " UmRxEntity::toPdcp - deliver SDU of " << pktAux->getByteLength() << " bytes to upper layer" << endl;
    send(pktAux, "out");
}

void UmRxEntity::rlcHandleD2DModeSwitchNr(bool oldConnection, bool oldMode, bool clearBuffer)
{
    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == UE && oldMode == IM) {
            EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - nothing to do on the DL leg of an IM flow" << endl;
            return;
        }
        if (clearBuffer) {
            // discard any partially-reassembled SDUs of the old mode
            sduBuffer->clearBuffer();
            sduBuffer->reset();
            if (t_ReassemblyTimer->isScheduled())
                cancelEvent(t_ReassemblyTimer);
        }
    }
    else {
        // new-mode entity: reset the SN window; the first PDU is forced in-sequence
        sduBuffer->reset();
        resetFlag_ = true;
    }
}

// ===================== shared =====================

void UmRxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer)
{
    Enter_Method_Silent("rlcHandleD2DModeSwitch()");
    if (soFraming_)
        rlcHandleD2DModeSwitchNr(oldConnection, oldMode, clearBuffer);
    else
        rlcHandleD2DModeSwitchLte(oldConnection, oldMode, clearBuffer);
}

bool UmRxEntity::isEmpty() const
{
    if (soFraming_)
        return sduBuffer == nullptr || sduBuffer->isEmpty();
    return buffered_.pkt == nullptr && pduBuffer_.size() == 0;
}

void UmRxEntity::handleBurst(BurstCheck event)
{
    // UL data-burst throughput (TS 136.314): track contiguous reception while the
    // RX buffer is continuously non-empty; report the burst on its end.
    simtime_t t1 = simTime();
    bool bufferEmpty = soFraming_
        ? (sduBuffer == nullptr || sduBuffer->isEmpty())
        : (pduBuffer_.size() + (buffered_.pkt == nullptr ? 0 : 1) == 0);

    if (bufferEmpty) {
        if (isBurst_) {
            if ((t1_ - t2_) > TTI && rlcMux_ && flowControlInfo_) {
                Throughput throughput = { totalBits_, (t1_ - t2_) };
                rlcMux_->addUeThroughput(flowControlInfo_->getSourceId(), throughput);
            }
            totalBits_ = 0;
            t2_ = 0;
            t1_ = 0;
            isBurst_ = false;
        }
    }
    else {
        if (isBurst_) {
            if (event == ENQUE) {
                totalBits_ += ttiBits_;
                t1_ = t1;
            }
        }
        else {
            isBurst_ = true;
            totalBits_ = ttiBits_;
            t2_ = t1;
            t1_ = t1;
        }
    }
    ttiBits_ = 0;
}

} //namespace
