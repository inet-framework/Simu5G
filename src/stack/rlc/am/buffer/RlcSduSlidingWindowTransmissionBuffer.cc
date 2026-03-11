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

#include "RlcSduSlidingWindowTransmissionBuffer.h"

namespace simu5g {

RlcSduSlidingWindowTransmissionBuffer::RlcSduSlidingWindowTransmissionBuffer(uint32_t windowSize, std::string name) : AM_Window_Size(windowSize), name(name){

}

RlcSduSlidingWindowTransmissionBuffer::~RlcSduSlidingWindowTransmissionBuffer() {
    auto it=txBuffer.begin();
    while(it!=txBuffer.end()) {

        if (it->second.sduPointer) {
            delete it->second.sduPointer;
        }
        ++it;
    }

    txBuffer.clear();
}
uint32_t RlcSduSlidingWindowTransmissionBuffer::addSdu(uint32_t length, Packet* sduPtr) {


    //take(sduPtr); // Take ownership
    uint32_t assignedSn = TX_Next;
    //Ownership and dropping of sduPtr is taken care in the constructor and destructor of  SduTxState
    txBuffer.emplace(assignedSn, SduTxState(assignedSn, length, sduPtr));

      //      << ". TX_Next moved to " << (TX_Next + 1) <<std::endl;

    TX_Next++;
    return assignedSn;
}

PendingSegment RlcSduSlidingWindowTransmissionBuffer::getSegmentForGrant(uint32_t grantSize) {
    PendingSegment result;
    // Iterate through buffer starting from the oldest unacknowledged SDU
    for (auto& pair : txBuffer) {
        uint32_t sn = pair.first;
        SduTxState& state = pair.second;

        // Protocol check: SN must be within [TX_Next_Ack, TX_Next_Ack + AM_Window_Size)
        if (sn < TX_Next_Ack || sn >= TX_Next_Ack + AM_Window_Size)  {
            continue;
        }

        uint32_t start, end;
        if (state.getNextSegment(grantSize, start, end)) {
            result.sn = sn;
            result.start = start;
            result.end = end;
            result.ptr = state.sduPointer;
            result.totalLength=state.totalLength;
            result.isValid = true;
            hasTransmitted=true;



            state.markTransmitted(start, end);
            highestSNTransmitted=std::max(highestSNTransmitted,sn);
            return result;
        }
    }

    return result;

}

std::set<uint32_t> RlcSduSlidingWindowTransmissionBuffer::handleAck(uint32_t sn, uint32_t start, uint32_t end,unsigned int poll_sn, bool& restartPoll) {
    std::set<uint32_t> acked;
    auto it = txBuffer.find(sn);
    if (it == txBuffer.end()) return acked;

    it->second.markAcked(start, end);

    if (it->second.isFullyAcked()) {
        // TODO: do we need to trigger a callback here?

        // To update TX_Next_Ack, we don't necessarily erase yet if it's not the bottom of the window,
        // but for memory management in this class, we track completeness.
    }


    // Update TX_Next_Ack: find smallest SN within [TX_Next_Ack, TX_Next] not fully acked
    uint32_t oldNextAck = TX_Next_Ack;
    while (TX_Next_Ack < TX_Next) {
        auto currentIt = txBuffer.find(TX_Next_Ack);

        // If the SDU is missing (already removed) or fully ACKed, we can advance
        if (currentIt == txBuffer.end() || currentIt->second.isFullyAcked()) {
            if (currentIt != txBuffer.end()) {
                delete currentIt->second.sduPointer;
                txBuffer.erase(currentIt); // Cleanup memory
                acked.insert(currentIt->first);
            }
            if (TX_Next_Ack==poll_sn) {
                restartPoll=true;
            }
            TX_Next_Ack++;
        } else {
            // Found the first unacknowledged SDU
            break;
        }
    }
/*
    if (TX_Next_Ack != oldNextAck) {
        std::cout <<simTime() <<";"<<name<< "[RLC TX] Window Slid. TX_Next_Ack moved from " << oldNextAck
               << " to " << TX_Next_Ack << std::endl;
    }
    */
    return acked;

}
int  RlcSduSlidingWindowTransmissionBuffer::getTotalPendingBytes() const {
    int totalPending = 0;
    for (const auto& pair : txBuffer) {
        const SduTxState& state = pair.second;
        uint32_t transmitted = state.getBytesTransmitted();
        if (transmitted < state.totalLength) {
            totalPending += (state.totalLength - transmitted);
        }
    }
    return totalPending;
}

PendingSegment RlcSduSlidingWindowTransmissionBuffer::getRetransmissionSegment(uint32_t sn, uint32_t start, uint32_t end, uint32_t grantSize) {
    PendingSegment segment;
    auto it = txBuffer.find(sn);
    if (it == txBuffer.end() || !hasTransmitted)  {
        segment.isValid=false;
        return segment;
    }

    ASSERT((end-start)>=0);
    uint32_t taskLen = end-start;
    uint32_t bytesToTransfer = std::min(grantSize, taskLen);

    segment.sn=sn;
    segment.start=start;
    segment.end=  segment.start + bytesToTransfer ;
    ASSERT(segment.end>=segment.start);
    segment.ptr=it->second.sduPointer;
    segment.totalLength=it->second.totalLength;
    segment.isValid=true;
    // Update the transmitted intervals tracking for this SDU
    it->second.markTransmitted(segment.start,  segment.end);

    //        << " [" << segment.start << ".." << segment.end << "]" << std::endl;

    return segment;
}

} /* namespace simu5g */
