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
#include <inet/common/ProtocolTag_m.h>
#include <inet/networklayer/common/NetworkInterface.h>

namespace simu5g {

Define_Module(NrPdcpRxEntity);



void NrPdcpRxEntity::initialize(int stage)
{
    LtePdcpRxEntity::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        reorderingEnabled_ = par("reorderingEnabled").boolValue();
        outOfOrderDelivery_ = par("outOfOrderDelivery").boolValue();
        rxWindowDesc_.windowSize_ = par("rxWindowSize");
        timeout_ = par("timeout").doubleValue();

        received_.resize(rxWindowDesc_.windowSize_, false);
        t_reordering_.setTimerId(REORDERING_T);
    }
}

void NrPdcpRxEntity::handlePdcpSdu(Packet *pdcpSdu, unsigned int sequenceNumber)
{
    Enter_Method("NrPdcpRxEntity::handlePdcpSdu");

    unsigned int rcvdSno = sequenceNumber;

    EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - processing PDCP SDU with SN[" << rcvdSno << "]" << endl;

    if (!reorderingEnabled_ || outOfOrderDelivery_) { // deliver packet to upper layer
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - Deliver SDU SN[" << rcvdSno << "] to upper layer" << endl;
        pdcpSdu->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
        deliverSduToUpperLayer(pdcpSdu);
        return;
    }
    // else dual connectivity is enabled and reordering needs to be done

    // check if already considered for reordering
    if (rcvdSno < rxWindowDesc_.rxDeliv_) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] <  was already considered for reordering. Discard the SDU" << endl;
        delete pdcpSdu;
        return;
    }

    // get the position in the buffer
    int index = rcvdSno - rxWindowDesc_.rxDeliv_;
    if (index >= rxWindowDesc_.windowSize_) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] <  is too large with respect to the window size. Advance the window and deliver out-of-sequence SDUs" << endl;
        delete pdcpSdu;
        return;
    }

    // check if already received
    if (received_.at(index)) {
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - the SN[" << rcvdSno << "] <  has already been received. Discard the SDU" << endl;
        delete pdcpSdu;
        return;
    }

    // update next expected sequence number
    if (rcvdSno >= rxWindowDesc_.rxNext_)
        rxWindowDesc_.rxNext_ = rcvdSno + 1;

    if (rcvdSno == rxWindowDesc_.rxDeliv_) {
        unsigned int oldRxDeliv = rxWindowDesc_.rxDeliv_;

        // this SDU is the next one to be delivered
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - Deliver SDU SN[" << rcvdSno << "] to upper layer" << endl;
        pdcpSdu->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
        deliverSduToUpperLayer(pdcpSdu);

        rxWindowDesc_.rxDeliv_++;

        // try to deliver in-order, buffered SDUs, if any
        int pos = 1;
        while (pos < rxWindowDesc_.windowSize_ && received_.at(pos)) {
            auto *sdu = check_and_cast<Packet *>(sduBuffer_.remove(pos));
            received_.at(pos) = false;

            EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - Deliver SDU buffered at index[" << pos << "] to upper layer" << endl;
            sdu->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
            deliverSduToUpperLayer(sdu);

            rxWindowDesc_.rxDeliv_++;
            pos++;
        }

        // shift the window by 'pos' positions. The slots still holding state are the
        // ones for SNs below rxNext_, i.e. old indices [pos, rxNext_ - oldRxDeliv):
        // the bound must be taken relative to rxDeliv_ as it was before the deliveries
        // above advanced it, otherwise the last 'pos' slots are neither moved nor
        // cleared and their stale flags later corrupt the window.
        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - shifting window by " << pos << " positions" << endl;
        ASSERT(rxWindowDesc_.rxNext_ >= rxWindowDesc_.rxDeliv_);
        for (unsigned int i = pos; i < rxWindowDesc_.rxNext_ - oldRxDeliv; ++i) {
            if (sduBuffer_.get(i) != nullptr)
                sduBuffer_.addAt(i - pos, sduBuffer_.remove(i));
            received_.at(i - pos) = received_.at(i);
            received_.at(i) = false;
        }
    }
    else {
        // else, buffer SDU

        EV << NOW << " NrPdcpRxEntity::handlePdcpSdu - SDU SN[" << rcvdSno << "] received out of sequence. Buffer at index[" << index << "]" << endl;

        sduBuffer_.addAt(index, pdcpSdu);
        received_.at(index) = true;
    }

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

        unsigned int old = rxWindowDesc_.rxDeliv_;

        // deliver buffered SDUs
        while (rxWindowDesc_.rxDeliv_ < rxWindowDesc_.rxReord_) {
            int pos = rxWindowDesc_.rxDeliv_ - old;
            if (received_.at(pos) == true) {
                EV << NOW << " NrPdcpRxEntity::handleMessage - Deliver SDU buffered at index[" << pos << "] to upper layer" << endl;
                auto *sdu = check_and_cast<Packet *>(sduBuffer_.remove(pos));
                sdu->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
                deliverSduToUpperLayer(sdu);
            }
            rxWindowDesc_.rxDeliv_++;
        }

        // deliver the in-sequence SDUs that follow, if any. Nothing was received at or
        // beyond rxNext_, so stop there: rxNext_ - old can equal windowSize_ (the SDU
        // at the last window slot arrived), and received_ has no slot at that index.
        while (rxWindowDesc_.rxDeliv_ < rxWindowDesc_.rxNext_ && received_.at(rxWindowDesc_.rxDeliv_ - old) == true) {
            EV << NOW << " NrPdcpRxEntity::handleMessage - Deliver SDU buffered at index[" << (rxWindowDesc_.rxDeliv_ - old) << "] to upper layer" << endl;
            auto *sdu = check_and_cast<Packet *>(sduBuffer_.remove(rxWindowDesc_.rxDeliv_ - old));
            sdu->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);
            deliverSduToUpperLayer(sdu);

            rxWindowDesc_.rxDeliv_++;
        }

        // shift window by 'i' positions
        int offset = rxWindowDesc_.rxDeliv_ - old;
        EV << NOW << " NrPdcpRxEntity::handleMessage - shifting window by " << offset << " positions" << endl;
        for (unsigned int i = offset; i < rxWindowDesc_.windowSize_; ++i) {
            if (sduBuffer_.get(i) != nullptr)
                sduBuffer_.addAt(i - offset, sduBuffer_.remove(i));
            received_.at(i - offset) = received_.at(i);
            received_.at(i) = false;
        }

        if (rxWindowDesc_.rxNext_ > rxWindowDesc_.rxDeliv_) {
            rxWindowDesc_.rxReord_ = rxWindowDesc_.rxNext_;
            t_reordering_.start(timeout_);
        }

        delete msg;
    }
}

} //namespace

