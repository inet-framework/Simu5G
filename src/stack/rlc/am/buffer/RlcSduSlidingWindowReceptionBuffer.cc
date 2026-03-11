/*
 * RLCSDUAssembler.cpp
 *
 *  Created on: Feb 10, 2026
 *      Author: eegea
 */

#include "RlcSduSlidingWindowReceptionBuffer.h"
#include "stack/rlc/am/packet/NrRlcAmDataPdu.h"

namespace simu5g {

RlcSduSlidingWindowReceptionBuffer::RlcSduSlidingWindowReceptionBuffer(uint32_t windowSize, std::string name) : AM_Window_Size(windowSize), name(name) {


}

RlcSduSlidingWindowReceptionBuffer::~RlcSduSlidingWindowReceptionBuffer() {
    auto it = sduBuffer.begin();
    while (it!=sduBuffer.end()) {
        if (it->second.sduPointer) {
            delete it->second.sduPointer;
        }
        ++it;
    }


}
std::pair<bool,bool> RlcSduSlidingWindowReceptionBuffer::handleSegment(uint32_t sn, uint32_t totalSduLength, uint32_t start, uint32_t end, Packet* sduDataPtr) {
    // 5.2.3.2.2: Check if SN falls outside receiving window
    // Window: [RX_Next, RX_Next + AM_Window_Size)
    auto pdu=sduDataPtr->peekAtFront<NrRlcAmDataPdu>();
    // Retrieve or create the SDU entry

    auto it = sduBuffer.find(sn);
    bool previous=true;
    if (it == sduBuffer.end()) {
        it = sduBuffer.emplace(sn, SduReassemblyState(totalSduLength, sduDataPtr)).first;
        previous=false;
    }

    // addSegment now returns whether any new bytes were added
    auto result = it->second.addSegment(start, end);
    bool newBytesAdded = result.first;
    bool newlyCompleted = (newBytesAdded && result.second);


    if (!newBytesAdded) {
        return {false,true};
    }



    //TODO: remove when implementation changes to a proper reassembly buffer of bytes
    //A fragment  is already present. Since we actually send the complete SDU in all packets not only fragments
    //we need to delete the previous one. Once it is fully completed the last one is deleted when passing up
    if (previous) {
        if(it->second.sduPointer) {
            delete it->second.sduPointer;
        }
        it->second.sduPointer=sduDataPtr;
    }
    // 5.2.3.2.3: Actions when an AMD PDU is placed in the reception buffer

    // Update RX_Next_Highest
    if (sn >= RX_Next_Highest) {
        RX_Next_Highest = sn + 1;
    }

    /*
    if (newlyCompleted) {
        std::cout << "[RLC RX] SDU SN=" << sn << " fully reassembled." << std::endl;
    }
*/
    return {newlyCompleted,false};
}
bool RlcSduSlidingWindowReceptionBuffer::hasMissingByteSegmentBeforeLast(uint32_t sn) {
    auto it = sduBuffer.find(sn);
    if (it == sduBuffer.end()) return false;
    return it->second.hasMissingByteSegmentBeforeLast();
}
void RlcSduSlidingWindowReceptionBuffer::updateRxHighestStatus() {
    uint32_t oldStatus = RX_Highest_Status;
    while (true) {
        auto it = sduBuffer.find(RX_Highest_Status);
        if (it != sduBuffer.end() && it->second.isComplete) {
            RX_Highest_Status++;
        } else {
            break;
        }
    }
    /*
    if (RX_Highest_Status != oldStatus) {
        std::cout << "[RLC RX] Updated RX_Highest_Status to " << RX_Highest_Status << std::endl;
    }
    */
}

void RlcSduSlidingWindowReceptionBuffer::updateRxNext() {
    uint32_t oldNext = RX_Next;
    while (true) {
        auto it = sduBuffer.find(RX_Next);
        if (it != sduBuffer.end() && it->second.isComplete) {
            //Remove now from buffer. Do not delete the pointer, it was deleted when the SDU was passed up
            discardSdu(RX_Next);
            RX_Next++;
        } else {
            break;
        }
    }
    /*
    if (RX_Next != oldNext) {
        std::cout << "[RLC RX] Updated RX_Next (Window Slide) to " << RX_Next << std::endl;

    }
    */
}

Packet*  RlcSduSlidingWindowReceptionBuffer::consumeSdu(uint32_t sn) {
    auto it = sduBuffer.find(sn);
    if (it != sduBuffer.end() && it->second.isComplete) {
        Packet* ptr = it->second.sduPointer;
        //Cannot erase here, only when we move RX_Next (window slide)
        it->second.sduPointer=nullptr;
        ++consumed;
        return ptr;
    }
    return nullptr;
}

void RlcSduSlidingWindowReceptionBuffer::handleReassemblyTimerExpiry(uint32_t RX_Next_Status_Trigger) {
    // Start searching from the trigger point
    uint32_t searchSn = RX_Next_Status_Trigger;

    while (true) {
        auto it = sduBuffer.find(searchSn);
        // If SDU is missing entirely OR it exists but is not complete
        if (it == sduBuffer.end() || !it->second.isComplete) {
            RX_Highest_Status = searchSn;
            break;
        }
        searchSn++;
    }


}


StatusPduData RlcSduSlidingWindowReceptionBuffer::generateStatusPduData() {
    StatusPduData data;
    uint32_t lastReportedSn = RX_Next - 1;

    // 1. Identify NACKs for SDUs in range [RX_Next, RX_Highest_Status)
    for (uint32_t sn = RX_Next; sn < RX_Highest_Status; ++sn) {
        auto it = sduBuffer.find(sn);

        // Case A: RLC SDU for which no byte segments have been received yet
        if (it == sduBuffer.end()) {
            // Check if we can group this into a NACK range
            if (!data.nacks.empty() && !data.nacks.back().isSegment &&
                    data.nacks.back().sn + data.nacks.back().nackRange == sn) {
                data.nacks.back().nackRange++;
            } else {
                NackInfo nack;
                nack.sn = sn;
                nack.isSegment = false;
                data.nacks.push_back(nack);
            }
        }
        // Case B: Partly received SDU - check for missing byte segments
        else if (!it->second.isComplete) {
            uint32_t currentByte = 0;
            it->second.receivedIntervals.sort();

            for (const auto& interval : it->second.receivedIntervals) {
                if (currentByte < interval.start) {
                    // Gap found between currentByte and start of interval
                    NackInfo nack;
                    nack.sn = sn;
                    nack.isSegment = true;
                    nack.soStart = currentByte;
                    nack.soEnd = interval.start - 1;
                    data.nacks.push_back(nack);
                }
                currentByte = interval.end + 1;
            }

            // Check for gap after the last received segment
            if (currentByte < it->second.totalLength) {
                NackInfo nack;
                nack.sn = sn;
                nack.isSegment = true;
                nack.soStart = currentByte;
                nack.soEnd = it->second.totalLength - 1; // Or special value for "end of SDU"
                data.nacks.push_back(nack);

            }
        }
    }

    // 2. Set ACK_SN to the SN of the next not received RLC SDU not indicated as missing
    // Start searching from RX_Highest_Status
    uint32_t ackSnCandidate = RX_Highest_Status;
    while (true) {
        auto it = sduBuffer.find(ackSnCandidate);
        // If it's complete, it's not the next "not received" SDU
        if (it != sduBuffer.end() && it->second.isComplete) {
            ackSnCandidate++;
        } else {
            // This is the first SDU that isn't fully in the buffer
            data.ackSn = ackSnCandidate;
            break;
        }
    }

    //        << ", NACK count=" << data.nacks.size() << std::endl;

    return data;
}


} /* namespace simu5g */
