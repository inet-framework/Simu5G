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

#ifndef STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_
#define STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_

#include <omnetpp.h>
#include <omnetpp/cobject.h>
#include <inet/common/packet/Packet.h>
#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <algorithm>

namespace simu5g {
using namespace inet;




/**
 * @brief Represents a range of bytes [start, end] for transmission tracking.
 */
struct TransmitInterval {
    uint32_t start;
    uint32_t end;

    bool operator<(const TransmitInterval& other) const {
        return start < other.start;
    }
};

/**
 * @brief Manages the transmission state of a single SDU.
 */
struct SduTxState {
    uint32_t sn;
    uint32_t totalLength;
    inet::Packet* sduPointer;

    // Intervals already transmitted (but not necessarily ACKed)
    std::list<TransmitInterval> transmittedIntervals;
    // Intervals successfully acknowledged by the peer
    std::list<TransmitInterval> acknowledgedIntervals;

    SduTxState(uint32_t s, uint32_t len, Packet* ptr)
    : sn(s), totalLength(len) {
        //take(ptr);
        sduPointer=ptr;
    }
    ~SduTxState() {
        //Do not delete the SDU, just nullptr, otherwise everytime the struct is copied it gets deleted
        //sduPointer=nullptr;
        /*if (sduPointer){
            drop(sduPointer);
            delete sduPointer;
            sduPointer=nullptr;
        }
        */

    }
    /**
     * @brief Calculates how many bytes of this specific SDU have already been transmitted.
     */
    uint32_t getBytesTransmitted() const {
        uint32_t count = 0;
        for (const auto& interval : transmittedIntervals) {
            count += (interval.end - interval.start + 1);
        }
        return count;
    }

    /**
     * @brief Finds the next available byte range that has not been transmitted.
     * @param grantSize Maximum bytes allowed by MAC.
     * @param outStart Output: starting byte index.
     * @param outEnd Output: ending byte index.
     * @return true if a segment (even partial) was found.
     */
    bool getNextSegment(uint32_t grantSize, uint32_t& outStart, uint32_t& outEnd) {
        if (grantSize == 0) return false;

        // Find the first gap in transmittedIntervals
        uint32_t nextByteToTx = 0;

        // Ensure intervals are sorted to find the first untransmitted gap
        transmittedIntervals.sort();

        for (const auto& interval : transmittedIntervals) {
            if (nextByteToTx < interval.start) {
                // Found a gap before this interval
                break;
            }
            nextByteToTx = interval.end + 1;
        }

        if (nextByteToTx >= totalLength) return false;

        outStart = nextByteToTx;
        uint32_t remainingInSdu = totalLength - nextByteToTx;
        uint32_t bytesToTransfer = std::min(grantSize, remainingInSdu);
        outEnd = outStart + bytesToTransfer - 1;

        return true;
    }
    /**
     * @brief Marks a range as transmitted.
     */
    void markTransmitted(uint32_t start, uint32_t end) {
        transmittedIntervals.push_back({start, end});
        mergeIntervals(transmittedIntervals);
    }

    /**
     * @brief Marks a range as acknowledged.
     */
    void markAcked(uint32_t start, uint32_t end) {
        acknowledgedIntervals.push_back({start, end});
        mergeIntervals(acknowledgedIntervals);
    }

    bool isFullyAcked() {
        if (acknowledgedIntervals.empty()) return false;
        mergeIntervals(acknowledgedIntervals);
        // If the first interval covers the whole range, it's fully ACKed
        return (acknowledgedIntervals.front().start == 0 &&
                acknowledgedIntervals.front().end == totalLength - 1);
    }

private:
    void mergeIntervals(std::list<TransmitInterval>& intervals) {
        if (intervals.empty()) return;
        intervals.sort();
        for (auto it = intervals.begin(); it != intervals.end(); ) {
            auto next = std::next(it);
            if (next != intervals.end() && next->start <= it->end + 1) {
                it->end = std::max(it->end, next->end);
                intervals.erase(next);
            } else {
                ++it;
            }
        }
    }
};
/**
 * @brief Structure representing a specific piece of an SDU for the MAC PDU.
 */
struct PendingSegment {
    //TODO: sn should use modules arithmetic since the SN is limited to 2048 when using 12 bit SN or 262143 when using 18 bit
    uint32_t sn;
    uint32_t start;
    uint32_t end;
    uint32_t totalLength;
    Packet* ptr;
    bool isValid = false;

};


class RlcSduSlidingWindowTransmissionBuffer: public omnetpp::cObject {
protected:
    std::map<uint32_t, SduTxState> txBuffer;
    uint32_t AM_Window_Size;   // Configurable window size
    std::string name;
    bool hasTransmitted=false;
    uint32_t highestSNTransmitted=0; //Initialize to a number not really transmitted
    // RLC AM State Variables
    uint32_t TX_Next = 0;      // Next SN to be assigned to a new SDU
    uint32_t TX_Next_Ack = 0;  // Smallest SN for which ACK is pending


public:
    RlcSduSlidingWindowTransmissionBuffer(uint32_t windowSize, std::string name );
    virtual ~RlcSduSlidingWindowTransmissionBuffer();


    /**
     * @brief Add a new SDU from upper layer.
     * Implements: associate SN = TX_Next and increment TX_Next.
     */
    uint32_t addSdu(uint32_t length, Packet* sduPtr);

    /**
     * @brief Extracts a single segment (or complete SDU) for the next transmission.
     * Per requirement: Only one SDU is considered per call.
     * * @param grantSize Total bytes available in the MAC PDU.
     * @return A PendingSegment containing data for at most one SDU.
     */
    PendingSegment getSegmentForGrant(uint32_t grantSize);


    /**
     * @brief Process an RLC Status PDU (ACK/NACK).
     */
    std::set<uint32_t> handleAck(uint32_t sn, uint32_t start, uint32_t end, unsigned int poll_sn, bool& restartPoll);


    /**
     * @brief Retrieves a segment for retransmission and updates the transmission tracking.
     * Since the retransmission size might differ from the original, this marks the new range.
     * @param sn The retransmission task SN
     * @param start The retransmission task segment start byte
     * @param end The retransmission task segment end byte
     * @param grantSize The current MAC grant size.
     * @return Segment  for retransmission if successful.
     */
    PendingSegment getRetransmissionSegment(uint32_t sn, uint32_t start, uint32_t end, uint32_t grantSize);



    /**
     * @brief Returns the total amount of data in bytes that hasn't been transmitted yet.
     */
    int getTotalPendingBytes() const;

    bool hasUnacknowledgedData() const {
        return TX_Next > TX_Next_Ack;
    }

    bool windowFull() const {
        // Check if SN falls within the transmitting window
        if (TX_Next >= TX_Next_Ack + AM_Window_Size) {
            std::cout << "[RLC TX] Window Full! Cannot add SDU. TX_Next=" << TX_Next
                    << ", TX_Next_Ack=" << TX_Next_Ack << std::endl;
            return true;
        }
        return false;
    }
    bool isInRtxRange(uint32_t sn) {
        return (sn>=TX_Next_Ack && sn<=highestSNTransmitted && hasTransmitted);
    }
    bool getSduData(uint32_t sn, Packet*& outPtr, uint32_t& outTotalLen) {
        auto it = txBuffer.find(sn);
        if (it == txBuffer.end()) {
            return false;
        }
        outPtr = it->second.sduPointer;
        outTotalLen = it->second.totalLength;
        return true;
    }
    bool isFullyAcknowledged(uint32_t sn) {
        auto it = txBuffer.find(sn);
        if (it == txBuffer.end()) {
            return false;
        }
        return (it->second.isFullyAcked());
    }
    // Accessors for state monitoring
    uint32_t getTxNext() const { return TX_Next; }
    uint32_t getTxNextAck() const { return TX_Next_Ack; }
    uint32_t getCurrentWindowSize() const {return (TX_Next-TX_Next_Ack);};
    uint32_t getHighestSNTransmitted() const {return   highestSNTransmitted;};



};

} /* namespace simu5g */

#endif /* STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_ */
