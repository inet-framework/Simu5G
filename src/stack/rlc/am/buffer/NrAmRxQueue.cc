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

#include "NrAmRxQueue.h"
#include "stack/rlc/am/packet/LteRlcAmPdu.h"
#include "stack/rlc/packet/LteRlcDataPdu.h"
#include "stack/rlc/am/packet/NrRlcAmDataPdu.h"
#include "stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"
#include "common/LteCommon.h"
#include "common/LteControlInfo.h"
#include "stack/mac/LteMacBase.h"
namespace simu5g {



Define_Module(NrAmRxQueue);
using namespace inet;

unsigned int NrAmRxQueue::totalCellRcvdBytes_ = 0;

simsignal_t NrAmRxQueue::rlcCellPacketLossSignal_[2] = { registerSignal("rlcCellPacketLossDl"), registerSignal("rlcCellPacketLossUl") };
simsignal_t NrAmRxQueue::rlcPacketLossSignal_[2] = { registerSignal("rlcPacketLossDl"), registerSignal("rlcPacketLossUl") };
simsignal_t NrAmRxQueue::rlcPduPacketLossSignal_[2] = { registerSignal("rlcPduPacketLossDl"), registerSignal("rlcPduPacketLossUl") };
simsignal_t NrAmRxQueue::rlcDelaySignal_[2] = { registerSignal("rlcDelayDl"), registerSignal("rlcDelayUl") };
simsignal_t NrAmRxQueue::rlcThroughputSignal_[2] = { registerSignal("rlcThroughputDl"), registerSignal("rlcThroughputUl") };
simsignal_t NrAmRxQueue::rlcPduDelaySignal_[2] = { registerSignal("rlcPduDelayDl"), registerSignal("rlcPduDelayUl") };
simsignal_t NrAmRxQueue::rlcPduThroughputSignal_[2] = { registerSignal("rlcPduThroughputDl"), registerSignal("rlcPduThroughputUl") };
simsignal_t NrAmRxQueue::rlcCellThroughputSignal_[2] = { registerSignal("rlcCellThroughputDl"), registerSignal("rlcCellThroughputUl") };

simsignal_t NrAmRxQueue::rlcThroughputSampleSignal_[2] = { registerSignal("rlcThroughputSampleDl"), registerSignal("rlcThroughputSampleUl") };
simsignal_t NrAmRxQueue::rlcCellThroughputSampleSignal_[2] = { registerSignal("rlcCellThroughputSampleDl"), registerSignal("rlcCellThroughputSampleUl") };
simsignal_t NrAmRxQueue::rxWindowOccupation= registerSignal("rxWindowOccupation");
simsignal_t NrAmRxQueue::rxWindowFull= registerSignal("rxWindowFull");


NrAmRxQueue::~NrAmRxQueue() {
    delete pduBuffer;
    cancelAndDelete(t_ReassemblyTimer);
    cancelAndDelete(t_StatusProhibitTimer);
    cancelAndDelete(throughputTimer);

}
void NrAmRxQueue::initialize() {
    totalRcvdBytes_ = 0;
    remoteEntity=nullptr;
    binder_.reference(this, "binderModule", true);
    lteRlc_.reference(this, "amModule", true);
    // Statistics
    LteMacBase *mac = inet::getConnectedModule<LteMacBase>(getParentModule()->gate("RLC_to_MAC"), 0);

    lastTputSample=NOW;
    tpsample=0;
    lastCellTputSample=NOW;
    cellTpsample=0;


    if (mac->getNodeType() == ENODEB || mac->getNodeType() == GNODEB) {
        dir_ = UL;
        name_entity = "GNB- "+std::to_string(lteRlc_->getId());;
    }
    else {
        dir_ = DL;
        name_entity = "UE- "+std::to_string(lteRlc_->getId());;
    }

    std::string name="rx-"+std::to_string(lteRlc_->getId());

    AM_window_size=par("AM_Window_Size");
    //Only two values are allowed by the standard: for 12 or 18 bit SN
    if (AM_window_size != static_cast<unsigned int>(2048)) {
        if (AM_window_size!= static_cast<unsigned int>(131072) ) {
            throw cRuntimeError("NrAmRxQueue::initialize() AM_window_size=%u, but only 2048 or 131072 are valid values ",AM_window_size);
        }

    }
    pduBuffer = new RlcSduSlidingWindowReceptionBuffer(AM_window_size, name_entity+"-rx-sliding window :");
    t_ReassemblyTimer = new cMessage("t_ReassemblyTimer");
    t_Reassembly=par("t_Reassembly");
    t_StatusProhibitTimer =new cMessage("t_StatusProhibitTimer");
    t_StatusProhibit=par("t_StatusProhibit");
    EV<<"t_StatusProhibit="<<t_StatusProhibit<<endl;
    //ackReportInterval_=t_StatusProhibit;

    rx_next_status_trigger=0;
    status_report_pending=false;
    throughputTimer= new cMessage("throughputTimer");
    throughputInterval=par("throughputInterval");

    scheduleAfter(throughputInterval,throughputTimer);
}



void NrAmRxQueue::enque(Packet *pkt)
{
    Enter_Method("enque()");


    take(pkt);

    auto pdu = pkt->peekAtFront<NrRlcAmDataPdu>();

    // Check if we need to extract FlowControlInfo for building up the matrix
    if (flowControlInfo_ == nullptr) {
        auto orig = pkt->getTag<FlowControlInfo>();
        // Make a copy of the original control info
        flowControlInfo_ = orig->dup();
        // Swap source and destination fields
        flowControlInfo_->setSourceId(orig->getDestId());
        flowControlInfo_->setSrcAddr(orig->getDstAddr());
        flowControlInfo_->setTypeOfService(orig->getTypeOfService());
        flowControlInfo_->setDestId(orig->getSourceId());
        flowControlInfo_->setDstAddr(orig->getSrcAddr());
        // Set up other fields
        flowControlInfo_->setDirection((orig->getDirection() == DL) ? UL : DL);
    }

    // Get the RLC PDU Transmission sequence number
    unsigned int sn = pdu->getPduSequenceNumber();


    //TS 138 322 5.2.3.2

    if (!pduBuffer->inWindow(sn) ) {
        //       << pduBuffer->getRxNext() << ".." << (pduBuffer->getRxNext() + AM_window_size - 1) << "])" << std::endl;

        //TS 138 322 5.3.4
        if (pdu->getPollStatus()) {
            sendStatusReport();
        }
        delete pkt;
        return;

    }
    if (pduBuffer->isReady(sn)) {
        //PDU has already been completed. Discard it
        //TS 138 322 5.3.4
        if (pdu->getPollStatus()) {
            sendStatusReport();
        }
        delete pkt;
        return;
    }

    int totalSduLength=pdu->getLengthMainPacket();
    int start=pdu->getStartOffset();
    int end=pdu->getEndOffset();

    auto segmentResult=pduBuffer->handleSegment(sn, totalSduLength, start, end, pkt);
    if (segmentResult.second) {
        //partial SDU already received. Discard it
        //TS 138 322 5.3.4
        if (pdu->getPollStatus()) {
            sendStatusReport();
        }
        delete pkt;
        return;
    }
    if (segmentResult.first) {
        //Pass up reassembled PDU
        passUp(sn);
        // Update RX_Highest_Status if x = RX_Highest_Status
        if (sn == pduBuffer->getRxHighestStatus()) {
            pduBuffer->updateRxHighestStatus();
        }

        // Update RX_Next if x = RX_Next
        if (sn == pduBuffer->getRxNext()) {
            pduBuffer->updateRxNext();
        }
    }
    //Check polling
    //TS 138 322 5.3.4
    if (pdu->getPollStatus()) {
        if (sn<pduBuffer->getRxHighestStatus() || pduBuffer->aboveWindow(sn) ){
            sendStatusReport();
        }
    }
    unsigned int current_rx_next=pduBuffer->getRxNext();
    bool hasHoles=pduBuffer->hasMissingByteSegmentBeforeLast(current_rx_next);

    //TS 138 322 5.2.3.2.3
    if (t_ReassemblyTimer->isScheduled()) {

        //Cancel the reassembly timer if: there is no SDU with missing fragments or no hole in the window

        bool noHolesAndStatus=false;
        if (current_rx_next +1 == rx_next_status_trigger) {
            if (!hasHoles) {
                noHolesAndStatus=true;
            } else {
            }
        }
        bool statusOff=false;
        if (!pduBuffer->inWindow(rx_next_status_trigger) && (rx_next_status_trigger!=(pduBuffer->getRxNext() +AM_window_size))) {
            statusOff=true;
        }
        if (current_rx_next== rx_next_status_trigger || noHolesAndStatus || statusOff ) {
            cancelEvent(t_ReassemblyTimer);

        }


    }
    //We do not put an else here, because this case includes the case we might have cancelled the timer just above
    if (!t_ReassemblyTimer->isScheduled()) {
        //We check:  if there is at least one SDU with fragments missing or if there is a hole in the rx window
        bool missingAndHole=false;

        if (pduBuffer->getRxNextHighest()==(current_rx_next+1)) {
            if (hasHoles) {
                missingAndHole=true;
            }
        }
        if ( (pduBuffer->getRxNextHighest()>(current_rx_next+1))|| missingAndHole ) {
            EV<<NOW<<"NrAmRxQueue::enque() t_ReassemblyTimer scheduled"<<endl;
            scheduleAfter(t_Reassembly,t_ReassemblyTimer);
            rx_next_status_trigger=pduBuffer->getRxNextHighest();

        }
    }
}





void NrAmRxQueue::passUp(const int seq_num)
{
    Enter_Method("passUp");


    Packet* bufferedPkt=pduBuffer->consumeSdu(seq_num);
    if (!bufferedPkt) {
        std::cout <<NOW  <<":"<< name_entity<<  "; Trying to pass up a null PDU with seq_num="<<seq_num<<std::endl;
        throw cRuntimeError("Trying to pass up a null PDU");
    }



    auto pdu=bufferedPkt->removeAtFront<NrRlcAmDataPdu>();

    size_t sduLengthPktLeng;
    if (pdu->getNumSdu()<1) {
        EV<<"pdu="<<pdu<<"bufferedPkt="<<bufferedPkt<<endl;
        std::cout<<"pdu="<<pdu<<"bufferedPkt="<<bufferedPkt<<endl;
        throw cRuntimeError("NrAmRxQueue::passUp(: the received PDU has no SDU ");
    }
    auto sdu = pdu->popSdu(sduLengthPktLeng);
    auto sduRlc=sdu->peekAtFront<LteRlcSdu>();
    EV << NOW << "  NrAmRxQueue::passUp() passing up SDU[" << sduRlc->getSnoMainPacket() << "] referenced by PDU with sn="<<seq_num<< endl;

    auto ci = sdu->getTag<FlowControlInfo>();

    auto pkt = sdu->removeAtFront<LteRlcSdu>();


    Direction dir = (Direction)ci->getDirection();
    MacNodeId dstId = ci->getDestId();
    MacNodeId srcId = ci->getSourceId();
    cModule *nodeb = nullptr;
    cModule *ue = nullptr;
    double delay = (NOW - sdu->getCreationTime()).dbl();

    if (dir == DL) {
        nodeb = getRlcByMacNodeId(binder_, srcId, AM);
        ue = getRlcByMacNodeId(binder_, dstId, AM);
    }
    else { // dir == one of UL, D2D, D2D_MULTI
        nodeb = getRlcByMacNodeId(binder_, dstId, AM);
        ue = getRlcByMacNodeId(binder_, srcId, AM);
    }

    totalRcvdBytes_ += sdu->getByteLength();
    totalCellRcvdBytes_ += sdu->getByteLength();
    double tputSample = (double)totalRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    double cellTputSample = (double)totalCellRcvdBytes_ / (NOW - getSimulation()->getWarmupPeriod());
    double interval=(NOW-lastCellTputSample).dbl();
    auto length=sdu->getByteLength();
    tpsample  +=length;
    cellTpsample +=length;
    if (nodeb != nullptr) {
        nodeb->emit(rlcCellThroughputSignal_[dir_], cellTputSample);
        nodeb->emit(rlcCellPacketLossSignal_[dir_], 0.0);
        if (interval>=1) {
            double tput = (double) cellTpsample/(interval);
            nodeb->emit(rlcCellThroughputSampleSignal_[dir_],tput);
            cellTpsample=0;
            lastCellTputSample=NOW;
        }
    }
    interval=(NOW-lastTputSample).dbl();
    if (ue != nullptr) {
        ue->emit(rlcThroughputSignal_[dir_], tputSample);
        ue->emit(rlcDelaySignal_[dir_], delay);
        ue->emit(rlcPacketLossSignal_[dir_], 0.0);
        /*
        if (interval>=1) {
            double tput = (double) tpsample/(interval);
            //EV<<NOW<<": AmRxQueue::passUp()"<<this<<";length="<<length<<"tpsample="<<tpsample<<"; rlcThroughputSample_="<<tput<<";interval="<<interval<<"; totalRcvdBytes_="<<totalRcvdBytes_<<"; time="<<(NOW - getSimulation()->getWarmupPeriod())<<endl;

            ue->emit(rlcThroughputSampleSignal_[dir_],tput);
            tpsample=0;
            lastTputSample=NOW;
        }
         */

    }

    // Get the SDU and pass it to the upper layers - PDU // SDU // PDCPPDU
    lteRlc_->sendDefragmented(sdu);
    passedUpSdu.insert(sduRlc->getSnoMainPacket());
    delete bufferedPkt;


}
void NrAmRxQueue::setRemoteEntity(MacNodeId id) {
    remoteEntity=getRlcByMacNodeId(binder_, id, AM);
}

void NrAmRxQueue::handleControlPdu(inet::Packet *pkt) {
    auto pdu = pkt->peekAtFront<NrRlcAmStatusPdu>();

    // Check if a control PDU has been received
    if (pdu->getAmType() != DATA) {
        if ((pdu->getAmType() == ACK)) {
            EV << NOW << " AmRxQueue::enque Received ACK message" << endl;
            // Forwarding ACK to associated transmitting entity
            lteRlc_->routeControlMessage(pkt);
        }  else {
            throw cRuntimeError("RLC AM - Unknown status PDU");
        }
        // Control PDU (not a DATA PDU) - completely processed

    } else {
        throw cRuntimeError("RLC AM - Unknown status PDU outside");
    }

}

void NrAmRxQueue::sendStatusReport() {
    Enter_Method("sendStatusReport()");

    // Check if the prohibit status report has been set.

    if (t_StatusProhibitTimer->isScheduled()) {
        EV << NOW
                << " NrAmRxQueue::sendStatusReport , minimum interval not reached "
                << t_StatusProhibit << endl;
        //<< t_StatusProhibit << endl;

        status_report_pending=true;

        return;
    }

    // Compute cumulative ACK
    StatusPduData data= pduBuffer->generateStatusPduData();
    // create a new RLC PDU
    auto pktPdu = new Packet("NR STATUS AM PDU)");
    auto pdu = makeShared<NrRlcAmStatusPdu>();
    // set RLC type descriptor
    pdu->setAmType(ACK);
    pdu->setData(data);
    //TODO: check the proper length
    //At the moment, according to TS 138 322 6.2.2.5
    //I will add one byte per NACK, and two bytes per SO and NACK range
    unsigned int size=3; //Corresponding to header + ACK_SN +E1
    for (unsigned int i=0; i<data.nacks.size(); ++i) {
        size++; //corresponding to the NACK
        if (data.nacks[i].isSegment) {
            size += 4; //2 for SOstart and SOend
        } else if (data.nacks[i].nackRange>1) {
            size++;
        }

    }
    pdu->setChunkLength(B(size));

    // set flowcontrolinfo
    *pktPdu->addTagIfAbsent<FlowControlInfo>() = *flowControlInfo_;
    // sending control PDU
    pktPdu->insertAtFront(pdu);
    EV << NOW <<";"<< lteRlc_<< "; NrAmRxQueue::sendStatusReport. Last sent " <<lastSentAck_<< endl;
    lteRlc_->bufferControlPdu(pktPdu);
    lastSentAck_ = NOW;
    scheduleAfter(t_StatusProhibit,t_StatusProhibitTimer);
    status_report_pending=false;

}

void NrAmRxQueue::handleMessage(cMessage* msg) {
    if (msg==t_ReassemblyTimer) {

        pduBuffer->handleReassemblyTimerExpiry(rx_next_status_trigger);
        bool hasHoles=pduBuffer->hasMissingByteSegmentBeforeLast(pduBuffer->getRxHighestStatus());
        bool restart=false;
        if (pduBuffer->getRxNextHighest()==pduBuffer->getRxHighestStatus()) {
            if (hasHoles) {
                restart=true;
            }

        }
        if ((pduBuffer->getRxNextHighest()>(pduBuffer->getRxHighestStatus()+1)) || restart ) {

            scheduleAfter(t_Reassembly,t_ReassemblyTimer);
            rx_next_status_trigger=pduBuffer->getRxNextHighest();
        }
        //TS 138 322 5.3.4
        sendStatusReport();
    }

    //TODO: It is not clear to me what the TS says about it. It seems that we just have to notify with a NACK,
    //but if the SDU has already been retransmitted more than the maxRtx threshold
    //both the rx window here and the tx window in the transmitting entity get stalled
    //The TS says that the tx entity should notify upper layers and I guess that they should discard the stalled SDU, but
    // it is not actually implemented in Simu5G (no method to notify PDCP-RRC)
    //Moreover, the tx entity may recover, but not the rx entity here, as far as I can see




    if (msg==t_StatusProhibitTimer) {
        if (status_report_pending) {
            sendStatusReport();
        }

    }
    if (msg==throughputTimer) {
        if (remoteEntity) {

            double tput = (double) tpsample/(throughputInterval);
            remoteEntity->emit(rlcThroughputSampleSignal_[dir_],tput);
            tpsample=0;

        }
        scheduleAfter(throughputInterval,throughputTimer);
    }
}

} //namespace
