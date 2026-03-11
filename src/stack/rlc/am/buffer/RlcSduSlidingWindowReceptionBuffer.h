/*
 * RLCSDUAssembler.h
 *
 *  Created on: Feb 10, 2026
 *      Author: eegea
 */

#ifndef STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWRECEPTIONBUFFER_H_
#define STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWRECEPTIONBUFFER_H_

#include "omnetpp.h"
#include <iostream>
#include <inet/common/packet/Packet.h>
#include "stack/rlc/LteRlcDefs.h"
#include <map>
#include <vector>
#include <list>
#include <algorithm>

namespace simu5g {
using namespace omnetpp;
using namespace inet;

/**
 * @brief Represents a range of bytes [start, end] within an SDU.
 */
struct ByteInterval {
    uint32_t start;
    uint32_t end;

    // Sort by start position for easy merging
    bool operator<(const ByteInterval& other) const {
        return start < other.start;
    }
};

/**
 * @brief Manages the reassembly state of a single SDU.
 */
struct SduReassemblyState {
    uint32_t totalLength;
    Packet* sduPointer; // Pointer to the actual simulation packet
    std::list<ByteInterval> receivedIntervals;
    bool isComplete;

    SduReassemblyState(uint32_t length , Packet* ptr )
    : totalLength(length), isComplete(false) {
        sduPointer=ptr;
    }

    /**
     * @brief Adds a new byte interval and merges overlapping or adjacent intervals.
     * @return A pair: <bool newlyBytesAdded, bool isNowComplete>
     */
    std::pair<bool, bool> addSegment(uint32_t start, uint32_t end) {
        if (isComplete) return {false, true};
        if (end >= totalLength) end = totalLength - 1;
        // Check if this interval is already fully covered by existing intervals
        bool isDuplicate = false;
        for (const auto& interval : receivedIntervals) {
            if (start >= interval.start && end <= interval.end) {
                isDuplicate = true;
                break;
            }
        }

        if (isDuplicate) {
            return {false, isComplete};
        }



        ByteInterval newInterval{start, end};

        // Find position to insert (sorted by start)
        auto it = std::lower_bound(receivedIntervals.begin(), receivedIntervals.end(), newInterval);
        receivedIntervals.insert(it, newInterval);

        // Merge logic: Iterate and merge overlaps
        for (auto current = receivedIntervals.begin(); current != receivedIntervals.end(); ) {
            auto next = std::next(current);
            if (next != receivedIntervals.end()) {
                // Check if overlapping or perfectly adjacent
                if (next->start <= current->end + 1) {
                    current->end = std::max(current->end, next->end);
                    receivedIntervals.erase(next);
                    // Don't increment current, try to merge the next one into expanded current
                } else {
                    ++current;
                }
            } else {
                break;
            }
        }

        // Check completion: If we have one interval covering 0 to totalLength - 1
        if (!receivedIntervals.empty()) {
            const auto& first = receivedIntervals.front();
            if (first.start == 0 && first.end == (totalLength - 1)) {
                isComplete = true;
            }
        }

        return {true,isComplete};
    }
    /**
     * @brief Checks if there is a missing byte segment before the last byte of all received segments.
     * This corresponds to the check needed for t-Reassembly stopping/starting logic.
     */
    bool hasMissingByteSegmentBeforeLast() const {
        if (receivedIntervals.empty()) return false;

        // If there is more than one interval, it means there is a gap between intervals.
        if (receivedIntervals.size() > 1) return true;

        // If there is only one interval, it must start at 0 to have no missing bytes before the end.
        return (receivedIntervals.front().start != 0);
    }
};

class RlcSduSlidingWindowReceptionBuffer : public cObject {
protected:
    // Map SDU SN to its reassembly state
    std::map<uint32_t, SduReassemblyState> sduBuffer;
    // RLC AM Reception State Variables
    uint32_t RX_Next = 0;            // Lower edge of the receiving window
    uint32_t RX_Next_Highest = 0;    // SN following the SN of the AMD PDU with the highest SN received
    uint32_t RX_Highest_Status = 0;  // SN following the SN of the highest SN SDU delivered to upper layer
    uint32_t AM_Window_Size;
    uint32_t consumed=0;
    std::string name;

public:
    RlcSduSlidingWindowReceptionBuffer(uint32_t windowSize, std::string name);
    virtual ~RlcSduSlidingWindowReceptionBuffer();
    /**
     * @brief Processes a received segment of an SDU.
     * * @param sn The SDU Sequence Number.
     * @param totalSduLength The total size of the original SDU.
     * @param start The start byte index of this segment.
     * @param end The end byte index of this segment.
     * @param sduDataPtr Pointer to the actual simulation object.
     * @return pair<completeSdu,discarded> if this segment caused the SDU to become fully assembled and if segment were discarded
     */
    std::pair<bool,bool> handleSegment(uint32_t sn, uint32_t totalSduLength, uint32_t start, uint32_t end, Packet* sduDataPtr);

    /**
     * @brief Retrieve the assembled SDU pointer and remove it from the buffer.
     */
    Packet* consumeSdu(uint32_t sn); 

    /**
     * @brief Implementation of the condition: "there is no missing byte segment of the SDU 
     * associated with SN = x before the last byte of all received segments of this SDU".
     */
    bool hasMissingByteSegmentBeforeLast(uint32_t sn) ;
    /**
     * @brief Set RX_Highest_Status to the next missing SDU 
     */
    void handleReassemblyTimerExpiry(uint32_t RX_Next_Status_Trigger);
    /**
     * @brief Generates data required for a STATUS PDU construction.
     */
    StatusPduData generateStatusPduData();
    /**
     * @brief Advance RX_Highest_Status to the first SDU > current for which not all bytes received.
     */
    void updateRxHighestStatus() ;
    /**
     * @brief Advance RX_Next to the first SDU > current for which not all bytes received.
     */
    void updateRxNext(); 

    /**
     * @brief Check if an SDU is ready for delivery.
     */
    bool isReady(uint32_t sn) const {
        auto it = sduBuffer.find(sn);
        return (it != sduBuffer.end() && it->second.isComplete);
    }

    bool inWindow(uint32_t sn) {
        if (sn < RX_Next || sn >= (RX_Next + AM_Window_Size)) {

            return false;
        }
        return true;
    }
    bool aboveWindow(uint32_t sn) {
        return (sn>=(RX_Next + AM_Window_Size));
    }
    /**
     * @brief Clean up old SDUs 
     */
    void discardSdu(uint32_t sn) {
        sduBuffer.erase(sn);
    }
    // Getters for state variables
    uint32_t getRxNext() const { return RX_Next; }
    uint32_t getRxNextHighest() const { return RX_Next_Highest; }
    uint32_t getRxHighestStatus() const { return RX_Highest_Status; }
};

} /* namespace simu5g */

#endif /* STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWRECEPTIONBUFFER_H_ */
