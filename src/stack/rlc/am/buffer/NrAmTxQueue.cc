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

#include "NrAmTxQueue.h"
//#include "stack/rlc/packet/LteRlcDataPdu.h"
#include "stack/rlc/am/packet/NrRlcAmDataPdu.h"
#include "stack/mac/buffer/LteMacBuffer.h"
#include "stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"


namespace simu5g {


Define_Module(NrAmTxQueue);

simsignal_t NrAmTxQueue::wastedGrantedBytes=registerSignal("wastedGrantedBytes");
simsignal_t NrAmTxQueue::enqueuedSduSize=registerSignal("enqueuedSduSize");
simsignal_t NrAmTxQueue::enqueuedSduRate=registerSignal("enqueuedSduRate");
simsignal_t NrAmTxQueue::requestedPduSize=registerSignal("requestedPduSize");
simsignal_t NrAmTxQueue::txWindowOccupation=registerSignal("txWindowOccupation");
simsignal_t NrAmTxQueue::txWindowFull=registerSignal("txWindowFull");
simsignal_t NrAmTxQueue::retransmissionPdu=registerSignal("retransmissionPdu");



NrAmTxQueue::~NrAmTxQueue() {
    cancelAndDelete(t_PollRetransmitTimer);
    delete txBuffer;
    delete rtxBuffer;
    while (!sduBuffer.empty()) {
        delete sduBuffer.front();
        sduBuffer.pop_front();
    }
    while (!controlBuffer.empty()) {
        delete controlBuffer.front();
        controlBuffer.pop_front();
    }
}
void NrAmTxQueue::initialize() {

    // Reference to corresponding RLC AM module
    lteRlc_.reference(this, "amModule", true);

    //TODO: conver to enum
    AM_window_size=par("AM_Window_Size");
    //Only two values are allowed by the standard: for 12 or 18 bit SN
    if (AM_window_size != static_cast<unsigned int>(2048)) {
        if (AM_window_size!= static_cast<unsigned int>(131072) ) {
            throw cRuntimeError("NrAmTxQueue::initialize() AM_window_size=%u, but only 2048 or 131072 are valid values ",AM_window_size);
        }

    }
    sn=0;
    radioLinkFailureDetected=false;
    tx_next_ack=0;
    pdu_without_poll=0;
    byte_without_poll=0;
    pollPDU=par("pollPDU");
    pollByte=par("pollByte");
    maxRtxThreshold=par("maxRtxThreshold");
    poll_sn=0;
    t_PollRetransmit= par("t_PollRetransmit");
    t_PollRetransmitTimer=    new cMessage("t_PollRetransmit timer");
    std::string name="tx-"+std::to_string(lteRlc_->getId());
    setPoll=false;

    mac=inet::getConnectedModule<LteMacBase>(getParentModule()->gate("RLC_to_MAC"), 0);
    if (mac->getNodeType() == ENODEB || mac->getNodeType() == GNODEB) {

        name_entity = "GNB- "+std::to_string(lteRlc_->getId());
    }
    else {

        name_entity = "UE- "+std::to_string(lteRlc_->getId());
    }
    txBuffer = new RlcSduSlidingWindowTransmissionBuffer(AM_window_size, name_entity+"-tx-sliding window :");
    rtxBuffer = new RlcSduRetransmissionBuffer(maxRtxThreshold);
    lastSduSample=NOW;
    sduSample=0;
    receivedSdus=0;
}

void NrAmTxQueue::enque(Packet *sdu) {
    Enter_Method("NrAmTxQueue::enque()"); // Direct Method Call
    EV << NOW << " NrAmTxQueue::enque() - inserting new SDU  " << sdu << endl;
    //EV << NOW << " NrAmTxQueue::enque() - inserting new SDU  " << sdu << endl;
    // Buffer the SDU
    //LteRlcAmSdu* sdu = pkt->peekAtFront<LteRlcAmSdu>();
    SDUInfo* si=new SDUInfo() ;
    //TODO: check this. I do not think we should keep a common control info. May it be that we receive SDUs with different info or is there an entity per connection?
    //In fact, there should only be a CID for all packets
    //TODO: we should take the tag for each sdu when transmitted
    lteInfo_ = sdu->getTag<FlowControlInfo>()->dup();
    infoCid_=ctrlInfoToMacCid(sdu->getTagForUpdate<FlowControlInfo>());

    take(sdu);
    si->sdu=sdu;
    sduBuffer.push_back(si);
    ++receivedSdus;
    //sduQueue_.insert(sdu);
    sduSample +=sdu->getByteLength();
    lteRlc_->emit(enqueuedSduSize, sdu->getByteLength());
    if ((NOW-lastSduSample)>=1) {
        lteRlc_->emit(enqueuedSduRate, sduSample/(NOW-lastSduSample));
        sduSample=0;
        lastSduSample=NOW;
    }


    lteRlc_->indicateNewDataToMac(sdu);

}

void NrAmTxQueue::sendPdus(int pduSize) {
    Enter_Method("NrAmTxQueue::sendPdus()");

    EV << NOW << " NrAmTxQueue::sendPdus( - PDU with size " << pduSize << " requested from MAC" << endl;
    lteRlc_->emit(requestedPduSize,pduSize);

    if (radioLinkFailureDetected) {
        std::cout<<NOW<<";"<<name_entity  <<";  NrAmTxQueue::sendPdus() RLF detected. Not sending more PDUs until connection is restored"<<endl;
        return;
    }


    // TODO: the request from MAC takes into account also the size of the RLC header
    //Different if using 12 or 18 bit SN; In both cases, it also depends on whether it has a segment or full PDU via SO
    //But RLC_HEADER_AM is used by the MAC schedulers... At the moment, we keep it this way..
    int size = pduSize- RLC_HEADER_AM;

    if (size<0) {
        //TODO: does it ever happen?
        //The only point of this packet seems to be to decrease requestedSdus_ in the MAC

        // send an empty (1-bit) message to notify the MAC that there is not enough space to send RLC PDU
        // (TODO: ugly, should be indicated in a better way)
        // create the RLC PDU
        auto pkt = new inet::Packet("lteRlcFragment size too small");
        auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();

        std::cout << NOW << " NrAmTxQueue::sendPdus() - cannot send PDU with data, pdulength requested by MAC (" << pduSize << "B) is too small." << std::endl;
        pkt->setName("lteRlcFragment (empty)");
        rlcPdu->setChunkLength(inet::b(1)); // send only a bit, minimum size
        pkt->insertAtFront(rlcPdu);
        lteRlc_->sendFragmented(pkt);
        return;
    }

    //Control PDU:  TS 38.322 indicates to prioritize control PDUs

    if (!controlBuffer.empty()) {
        auto pktControl = check_and_cast<inet::Packet *>(controlBuffer.front());
        controlBuffer.pop_front();

        EV << NOW << " NrAmTxQueue::sendPdus() - sending Control PDU" << pktControl<< " with size " << pktControl->getByteLength() << " bytes to lower layer" << endl;
        //The PDU is already well formed before introduced in the buffer,so we just send
        lteRlc_->sendFragmented(pktControl);

        // TODO should we send more PDUs at this point if there is enough room in the requested size. The implementation of the LteMac seems to exclude this,
        //since it only asks for one PDU and throws an error if more are sent. But then probably part of the grant is wasted. The MAC should ask for more PDUs then

        // TODO this is another workaround, to avoid getting stalled. MAC may have asked a pdu size equal to all the pending
        // data in its macBuffers, and so think that our queues are empty, but we can only send a STATUS PDU
        unsigned int pendingData=getPendingDataVolume();
        if (pendingData>0 ) {
            // create the RLC PDU
            if (lteInfo_) {
                auto pkt = new inet::Packet("lteRlcFragment -Indicate new data");
                auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();
                EV << NOW << " NrAmTxQueue::sendPdus( - Informing MAC of pending "<<pendingData << endl;
                rlcPdu->setChunkLength(B(pendingData));
                *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
                pkt->insertAtFront(rlcPdu);
                lteRlc_->indicateNewDataToMac(pkt);
                delete pkt;
            }
            //TODO: what if lteInfo is not set yet? It may happen after a RLF, we receive an on flight PDU from previous entity and our rxqueue sends a control PDU
        }
        return;
    }

    //Now retransmissions first. The stored PDU is already formed, so we do not need to substract the header.
    if (sendRetransmission((size))) {
        reportBufferStatus();
        return;
    }
    PendingSegment segment;
    segment.isValid=false;
    //Now check if we have pending data to send in the txBuffer that fits in the grant
    if (txBuffer->getTotalPendingBytes()>0) {
        segment=txBuffer->getSegmentForGrant(size);

    }
    if (!segment.isValid) {
        //This may be that we do not have pending data in the transmission buffer or have not found a segment
        //Now try to detach a new SDU
        if (sduBuffer.empty()) {
            EV << NOW << " NrAmTxQueue::sendPdus()  Requested PDU but buffer is empty. sduBuffer.size()= " << sduBuffer.size() << " bytes, size="<<size<< endl;

            lteRlc_->emit(wastedGrantedBytes, size);
            //delete pkt;
            return;
        } else {

            //Try to detach data from the SDU buffer
            SDUInfo* si=sduBuffer.front();
            auto bufferedSdu = check_and_cast<inet::Packet *>(si->sdu);
            auto rlcSdu = bufferedSdu->peekAtFront<LteRlcSdu>();
            int sduSequenceNumber = rlcSdu->getSnoMainPacket();
            int sduLength = rlcSdu->getLengthMainPacket(); // length without the SDU header

            //<< "; length encapsulated packet=" << sduLength << endl;

            if (txBuffer->windowFull()) {
                lteRlc_->emit(txWindowFull,1);

                //TODO: check if we need to inform the MAC here of our data volume and if
                return;

            }
            //Add to transmission buffer
            txBuffer->addSdu(sduLength, bufferedSdu);
            //Remove from the sduBuffer
            sduBuffer.pop_front();
            //Do not delete yet, it is deleted when ACKed
            segment =txBuffer->getSegmentForGrant(size);
        }
    }

    //Now try to transmit the SDU or segment

    if (!segment.isValid) {
        EV << NOW << " NrAmTxQueue::sendPdus() Not found any SDU segment fit to transmit. sduBuffer.size()= " << sduBuffer.size() << " bytes, size="<<size<< endl;
        std::cout<<name_entity  << NOW << " NrAmTxQueue::sendPdus()  Not found any SDU segment fit to transmit. sduBuffer.size()= " << sduBuffer.size() << " bytes, size="<<size<< endl;

        lteRlc_->emit(wastedGrantedBytes, size);

        return;
    }
    //We have found a segment that fits the grant
    sendSegment(segment);

    lteRlc_->emit(txWindowOccupation,(txBuffer->getTxNext()-txBuffer->getTxNextAck()));
    /////
    //TODO: workaround, check what the MAC thinks there is in the buffers

    reportBufferStatus();



}

void NrAmTxQueue::sendSegment(PendingSegment segment) {
    // create the RLC PDU

    auto rlcPdu = inet::makeShared<NrRlcAmDataPdu>();

    // compute SI
    // the meaning of this field is specified in 3GPP TS 36.322 but is different in TS 38.322, called SI, but we reuse the FramingInfo field

    FramingInfo fi = 0; //00 //full sdu
    unsigned short int mask;
    uint32_t segmentSize= segment.end-segment.start;
    if (segmentSize==segment.totalLength) {
        //Correctness check
    } else {
        if (segment.start==0) {
            //start segment
            mask = 1;   // 01
            fi |= mask;
        } else if (segment.end==(segment.totalLength-1)) {
            //end segment
            mask = 2;   // 10
            fi |= mask;
        } else {
            //middle segment
            mask =3; //11
            fi |=mask;
        }
    }
    //Retrieve SDU
    auto bufferedSdu = check_and_cast<inet::Packet *>(segment.ptr);
    auto rlcSdu = bufferedSdu->peekAtFront<LteRlcSdu>();
    int sduLength = rlcSdu->getLengthMainPacket(); // length without the SDU header
    unsigned int pduSequenceNumber = segment.sn;
    //Update max sn if needed
    sn=std::max(sn,pduSequenceNumber);
    int sduSequenceNumber = rlcSdu->getSnoMainPacket();
    rlcPdu->pushSdu(bufferedSdu->dup(), segmentSize);
    //Finish PDU
    rlcPdu->setFramingInfo(fi);
    rlcPdu->setPduSequenceNumber(pduSequenceNumber);
    rlcPdu->setChunkLength(inet::B(RLC_HEADER_AM + segmentSize));
    rlcPdu->setSnoMainPacket(sduSequenceNumber);
    rlcPdu->setLengthMainPacket(sduLength);
    rlcPdu->setStartOffset(segment.start);
    rlcPdu->setEndOffset(segment.end);
    //Polling
    ++pdu_without_poll;
    byte_without_poll += segmentSize;
    bool poll=checkPolling();
    rlcPdu->setPollStatus(poll);
    std::string n_p="NR AM RLC Fragment -" + std::to_string(pduSequenceNumber);
    auto pkt = new inet::Packet(n_p.c_str());
    pkt->insertAtFront(rlcPdu);
    // TODO instead of using a common lteInfo, each packet receives the one carried by the SDU
    if (lteInfo_) {
        *(pkt->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
    } else {
        //TODO: something like this
        *(pkt->addTagIfAbsent<FlowControlInfo>())=*(rlcSdu->getTag<FlowControlInfo>());
    }



    lteRlc_->sendFragmented(pkt);


}
bool NrAmTxQueue::checkPolling() {
    //Polling
    bool noPendingData= (txBuffer->getTotalPendingBytes()==0 && sduBuffer.empty() && rtxBuffer->getRetxPendingBytes()==0);

    if ( setPoll || (pdu_without_poll>pollPDU) || (byte_without_poll>=pollByte) || noPendingData|| txBuffer->windowFull()) {

        pdu_without_poll=0;
        byte_without_poll=0;
        poll_sn=sn;

        rescheduleAfter(t_PollRetransmit,t_PollRetransmitTimer);
        setPoll=false;
        return true;

    }
    return false;


}
void  NrAmTxQueue::reportBufferStatus() {

    auto vbuf= mac->getMacBuffers();

    unsigned int macOccupancy = vbuf->at(infoCid_)->getQueueOccupancy();


    if (macOccupancy==0) {
        unsigned int pendingData=getPendingDataVolume();
        if (pendingData>0) {

            //If we do not have a lteInfo we cannot inform the MAC of the pending volume because the CID is taken from this
            //TODO: change to a more robust approach
            if (lteInfo_) {
                // create the RLC PDU to indicate new data
                auto pktInform = new inet::Packet("lteRlcFragment Inform MAC");
                auto rlcPduInform = inet::makeShared<NrRlcAmDataPdu>();
                rlcPduInform->setChunkLength(B(pendingData));
                //pkt->copyTags(*pktAux);
                *(pktInform->addTagIfAbsent<FlowControlInfo>()) = *lteInfo_;
                pktInform->insertAtFront(rlcPduInform);
                //pkt->setByteLength(sduLength);
                lteRlc_->indicateNewDataToMac(pktInform);
                delete pktInform;
            }

        }

    }


}

bool NrAmTxQueue::sendRetransmission(int size) {
    RetxTask next;

    if (rtxBuffer->getNextRetxTask(next)) {
        uint32_t rtxSn=next.sn;
        uint32_t start=next.soStart;
        uint32_t end=next.soEnd;

        if (next.isWholeSdu) {
            Packet* ptr=nullptr;
            uint32_t totalLength=0;
            bool aux=txBuffer->getSduData(rtxSn, ptr, totalLength);
            if (aux) {
                start=0;
                end=totalLength-1;
            } else {
                if (radioLinkFailureDetected){
                    return false;
                }
                std::cout<<"NrAmTxQueue::sendRetransmission whole SDU with sn="<<rtxSn<<" retrieved for retransmission not found"<<endl;
                throw cRuntimeError("NrAmTxQueue::sendRetransmission whole SDU with sn=%u  retrieved for retransmission not found",rtxSn);
                return false;
            }
        }

        PendingSegment segment =txBuffer->getRetransmissionSegment(rtxSn, start , end, size);
        if (!segment.isValid) {
            //We have not found the SDU
            if (radioLinkFailureDetected){
                return false;
            }
            throw cRuntimeError("NrAmTxQueue::sendRetransmission A missing SDU with sn=%u has been retrieved for retransmission",rtxSn);
            return false;
        }
        if (!segment.ptr) {
            //Found SDU but points to null
            if (radioLinkFailureDetected){
                return false;
            }
            throw cRuntimeError("NrAmTxQueue::sendRetransmission A  SDU with sn=%u  retrieved for retransmission points to null",rtxSn);
            return false;
        }
        sendSegment(segment);


        rtxBuffer->markRetransmitted(next);

        lteRlc_->emit(retransmissionPdu,1);
        return true;
    }
    return false;



}
void NrAmTxQueue::handleControlPacket(omnetpp::cPacket *pkt) {
    Enter_Method("handleControlPacket()");

    take(pkt);

    auto pktPdu = check_and_cast<Packet *>(pkt);
    auto pdu = pktPdu->peekAtFront<NrRlcAmStatusPdu>();
    StatusPduData data = pdu->getData();


    rtxBuffer->beginStatusPduProcessing();
    std::set<uint32_t> nacks;
    bool restartPoll=false;
    for (long unsigned int i = 0; i < data.nacks.size(); ++i) {
        NackInfo info=data.nacks[i];

        unsigned int j=0;
        while (j<info.nackRange) {
            if (txBuffer->isInRtxRange(info.sn+j)) {
                //Beware: we have implemented NACKs to say if they are segment, but we are storing in the RetxTask if they are whole.. so the boolean is inverted
                bool isWhole=!info.isSegment;

                bool added= rtxBuffer->addNack(info.sn+j, isWhole, info.soStart, info.soEnd);
                if (!added) {
                    //TODO: Check this we have reached the maximum number of retransmissions. We should notify Radio Link Failure to the upper layers..
                    //At the moment just inform and let it delete connections
                    std::cout<< name_entity <<"[CRITICAL] Calling radio link failure"<<endl;
                    radioLinkFailureDetected=true;
                    if (lteInfo_) { //Just in case
                        lteRlc_->handleRadioLinkFailure(lteInfo_);
                    } else {
                        //otherwise, we would need to get the info from the stored SDU
                        Packet* ptrAux=nullptr;
                        uint32_t totalLength=0;
                        bool aux=txBuffer->getSduData(info.sn+j, ptrAux, totalLength);
                        if (aux) {
                            lteRlc_->handleRadioLinkFailure(ptrAux->getTag<FlowControlInfo>()->dup());
                        } //TODO: What if not found? Again the CID should be permanent, not depending on the FlowControlInfo
                    }
                    //Inform just once
                    delete pkt;
                    return;

                }
            }
            nacks.insert(info.sn+j);
            //TS 138 322 5.3.3
            if ((info.sn+j)==poll_sn) {
                restartPoll=true;
            }
            ++j;
        }
    }
    //Now process ACK
    uint32_t next=txBuffer->getTxNextAck();
    while (next<data.ackSn) {
        uint32_t totalLength;
        Packet* sdu=nullptr;
        auto it=nacks.find(next);
        uint32_t oldnext=next;
        if (it==nacks.end()) {
            if (txBuffer->getSduData(next,sdu, totalLength)) {
                std::set<uint32_t> acked=txBuffer->handleAck(next, 0, totalLength-1, poll_sn, restartPoll);
                //Remove from rtxBuffer
                auto it=acked.begin();
                while(it!=acked.end()) {
                    rtxBuffer->clearSdu(*it);
                    ++it;
                }
            } else {
                //TODO: Now, what happens if we do not have the SDU in the buffer...?
                //throw cRuntimeError("NrAmTxQueue::handleControlPacket A missing SDU with sn=%u has been ACKed",next);
            }
        }
        //TS 138 322 5.3.3
        /*
        if (next==poll_sn) {
            restartPoll=true;
        }*/

        ++next;
    }


    //TS 138 322 5.3.3
    if (t_PollRetransmitTimer->isScheduled() && restartPoll) {
        rescheduleAfter(t_PollRetransmit,t_PollRetransmitTimer);
    }

    lteRlc_->emit(txWindowOccupation,txBuffer->getCurrentWindowSize());
    delete pkt;

}


void NrAmTxQueue::bufferControlPdu(omnetpp::cPacket *pkt) {
    Enter_Method("NrAmTxQueue::bufferControlPdu()"); // Direct Method Call
    take(pkt); // Take ownership
    controlBuffer.push_back(pkt);
    lteRlc_->indicateNewDataToMac(pkt);

}

void NrAmTxQueue::finish() {
}

void NrAmTxQueue::handleMessage(cMessage *msg) {
    if (msg==t_PollRetransmitTimer) {
        setPoll=true;
        bool noPendingData= (txBuffer->getTotalPendingBytes()==0 && sduBuffer.empty() && rtxBuffer->getRetxPendingBytes()==0);
        if ( noPendingData || txBuffer->windowFull()) {
            uint32_t hsn=txBuffer->getHighestSNTransmitted();
            Packet* ptr=nullptr;
            uint32_t totalLength=0;
            bool found=txBuffer->getSduData(hsn, ptr, totalLength);
            bool added=false;
            if (found) {
                added= rtxBuffer->addNack(hsn, true, 0, totalLength-1);
            }
            //Consider any not ACK PDU for retransmission
            if (!found || !added) {
                uint32_t next=txBuffer->getTxNextAck();
                while (txBuffer->isInRtxRange(next)) {
                    if (!txBuffer->isFullyAcknowledged(next)) {
                        bool aux=txBuffer->getSduData(next, ptr, totalLength);
                        if (aux) {
                            if(rtxBuffer->addNack(next, true, 0, totalLength-1)) {
                                return;
                            }
                        }

                    }
                    ++next;

                }
            }
        }
    }
}

unsigned int NrAmTxQueue::getPendingDataVolume() const {
    unsigned int size=0;
    for (unsigned int i=0;i<sduBuffer.size(); ++i) {
        SDUInfo* si=sduBuffer.front();
        auto pktAux = check_and_cast<inet::Packet *>(si->sdu);
        auto rlcSdu = pktAux->peekAtFront<LteRlcSdu>();
        size += rlcSdu->getLengthMainPacket();
    }
    size += txBuffer->getTotalPendingBytes();
    size += rtxBuffer->getRetxPendingBytes();
    //Control buffer
    size += (controlBuffer.size()*2);
    for (unsigned int i=0;i<controlBuffer.size(); ++i) {
        auto p=check_and_cast<Packet*>(controlBuffer.front())->peekAtFront<NrRlcAmStatusPdu>();
        size+=p->getChunkLength().get();
    }
    return size;

}

} //namespace
