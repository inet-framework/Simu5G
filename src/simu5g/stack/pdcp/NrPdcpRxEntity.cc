//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/pdcp/NrPdcpRxEntity.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include <inet/networklayer/common/NetworkInterface.h>

namespace simu5g {

Define_Module(NrPdcpRxEntity);



NrPdcpRxEntity::~NrPdcpRxEntity()
{
    for (auto& [count, sdu] : reorderingBuffer_)
        delete sdu;
}

void NrPdcpRxEntity::initialize(int stage)
{
    LtePdcpRxEntity::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        outOfOrderDelivery_ = par("outOfOrderDelivery").boolValue();
        rxWindowDesc_.windowSize_ = par("rxWindowSize");
        timeout_ = par("timeout").doubleValue();

        t_reordering_.setTimerId(REORDERING_T);

        WATCH_EXPR("reorderingBuffer", reorderingBuffer_.size());
        WATCH(rxWindowDesc_);
        WATCH(t_reordering_);
    }
}

void NrPdcpRxEntity::handlePdcpSdu(Packet *pdcpSdu, unsigned int sequenceNumber)
{
    Enter_Method("NrPdcpRxEntity::handlePdcpSdu");

    unsigned int rcvdSno = sequenceNumber;

    EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - processing PDCP SDU with SN[" << rcvdSno << "]" << endl;

    // already delivered, or considered lost by an expired t-Reordering
    if (rcvdSno < rxWindowDesc_.rxDeliv_) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] is below RX_DELIV. Discard the SDU" << endl;
        delete pdcpSdu;
        return;
    }

    if (rcvdSno - rxWindowDesc_.rxDeliv_ >= rxWindowDesc_.windowSize_) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] <  is too large with respect to the window size. Advance the window and deliver out-of-sequence SDUs" << endl;
        delete pdcpSdu;
        return;
    }

    // received before (a stored SDU, or one already delivered out of order)
    if (reorderingBuffer_.count(rcvdSno) != 0) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] has already been received. Discard the SDU" << endl;
        delete pdcpSdu;
        return;
    }

    // update next expected sequence number
    if (rcvdSno >= rxWindowDesc_.rxNext_)
        rxWindowDesc_.rxNext_ = rcvdSno + 1;

    // with outOfOrderDelivery, the SDU goes up at once, and its COUNT stays in the buffer
    // (as an empty entry) until RX_DELIV passes it, so that a duplicate is recognized
    if (outOfOrderDelivery_) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - Deliver SDU SN[" << rcvdSno << "] to upper layer out of order" << endl;
        deliverSduToUpperLayer(pdcpSdu);
        reorderingBuffer_[rcvdSno] = nullptr;
    }
    else {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - Store SDU SN[" << rcvdSno << "]" << endl;
        reorderingBuffer_[rcvdSno] = pdcpSdu;
    }

    // the SDU at RX_DELIV, and the stored ones in sequence after it, go up
    if (rcvdSno == rxWindowDesc_.rxDeliv_)
        deliverConsecutiveSdus();

    // handle t-reordering

    // if t_reordering is running
    if (t_reordering_.busy()) {
        if (rxWindowDesc_.rxDeliv_ >= rxWindowDesc_.rxReord_)
            t_reordering_.stop();
    }
    // if t_reordering is not running
    if (!t_reordering_.busy()) {
        if (rxWindowDesc_.rxDeliv_ < rxWindowDesc_.rxNext_) {
            t_reordering_.start(timeout_);
            rxWindowDesc_.rxReord_ = rxWindowDesc_.rxNext_;
        }
    }
}

void NrPdcpRxEntity::deliverConsecutiveSdus()
{
    for (auto it = reorderingBuffer_.find(rxWindowDesc_.rxDeliv_); it != reorderingBuffer_.end() && it->first == rxWindowDesc_.rxDeliv_; it = reorderingBuffer_.erase(it)) {
        if (it->second != nullptr) {
            EV << NOW << " NrPdcpRxEntity - Deliver stored SDU SN[" << it->first << "] to upper layer" << endl;
            deliverSduToUpperLayer(it->second);
        }
        rxWindowDesc_.rxDeliv_++;
    }
}

bool NrPdcpRxEntity::isEmpty() const
{
    for (const auto& [count, sdu] : reorderingBuffer_)
        if (sdu != nullptr)
            return false;
    return true;
}

void NrPdcpRxEntity::handleMessage(cMessage *msg)
{
    if (!msg->isSelfMessage()) {
        // Packet from gate — delegate to base class handler
        PdcpRxEntityBase::handleMessage(msg);
        return;
    }
    if (msg->isName("timer")) {
        t_reordering_.handle();

        EV << NOW << " NrPdcpRxEntity::handleMessage : t_reordering timer has expired " << endl;

        // The timer runs only while an SDU below RX_REORD is missing, and a reception that
        // advances RX_DELIV to RX_REORD stops it
        ASSERT(rxWindowDesc_.rxDeliv_ < rxWindowDesc_.rxReord_);

        // deliver the stored SDUs below RX_REORD, in ascending COUNT order
        for (auto it = reorderingBuffer_.begin(); it != reorderingBuffer_.end() && it->first < rxWindowDesc_.rxReord_; it = reorderingBuffer_.erase(it)) {
            if (it->second != nullptr) {
                EV << NOW << " NrPdcpRxEntity::handleMessage - Deliver stored SDU SN[" << it->first << "] to upper layer" << endl;
                deliverSduToUpperLayer(it->second);
            }
        }

        // then the stored SDUs in sequence from RX_REORD
        rxWindowDesc_.rxDeliv_ = rxWindowDesc_.rxReord_;
        deliverConsecutiveSdus();

        if (rxWindowDesc_.rxNext_ > rxWindowDesc_.rxDeliv_) {
            rxWindowDesc_.rxReord_ = rxWindowDesc_.rxNext_;
            t_reordering_.start(timeout_);
        }

        delete msg;
    }
}

} //namespace

