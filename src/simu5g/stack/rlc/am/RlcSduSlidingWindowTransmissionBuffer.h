//
//                  Simu5G
//
// Authors: Esteban Egea Lopez (Universidad Politecnica de Cartagena)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_
#define STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include <algorithm>
#include <list>
#include <map>
#include <set>

namespace simu5g {




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
    inet::Packet *sduPointer;

    // Intervals already transmitted (but not necessarily ACKed)
    std::list<TransmitInterval> transmittedIntervals;
    // Intervals successfully acknowledged by the peer
    std::list<TransmitInterval> acknowledgedIntervals;

    SduTxState(uint32_t s, uint32_t len, inet::Packet *ptr)
        : sn(s), totalLength(len), sduPointer(ptr) {}
    ~SduTxState() = default;
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
    // Offset of the first not-yet-transmitted byte (== totalLength if fully sent).
    uint32_t getNextByteToTx() const {
        uint32_t nextByteToTx = 0;
        std::list<TransmitInterval> sorted = transmittedIntervals;
        sorted.sort();
        for (const auto &interval : sorted) {
            if (nextByteToTx < interval.start)
                break;
            nextByteToTx = interval.end + 1;
        }
        return nextByteToTx;
    }

    bool getNextSegment(uint32_t grantSize, uint32_t &outStart, uint32_t &outEnd) {
        if (grantSize == 0)
            return false;

        uint32_t nextByteToTx = 0;
        transmittedIntervals.sort();

        for (const auto &interval : transmittedIntervals) {
            if (nextByteToTx < interval.start)
                break;
            nextByteToTx = interval.end + 1;
        }

        if (nextByteToTx >= totalLength)
            return false;

        outStart = nextByteToTx;
        uint32_t remainingInSdu = totalLength - nextByteToTx;
        outEnd = outStart + std::min(grantSize, remainingInSdu) - 1;
        return true;
    }
    /**
     * @brief Marks a range as transmitted.
     */
    void markTransmitted(uint32_t start, uint32_t end) {
        transmittedIntervals.push_back({start, end});
        mergeIntervals(transmittedIntervals);
    }

    void markAcked(uint32_t start, uint32_t end) {
        acknowledgedIntervals.push_back({start, end});
        mergeIntervals(acknowledgedIntervals);
    }

    bool isFullyAcked() {
        if (acknowledgedIntervals.empty())
            return false;
        mergeIntervals(acknowledgedIntervals);
        return (acknowledgedIntervals.front().start == 0 &&
                acknowledgedIntervals.front().end == totalLength - 1);
    }

  private:
    void mergeIntervals(std::list<TransmitInterval> &intervals) {
        if (intervals.empty())
            return;
        intervals.sort();
        for (auto it = intervals.begin(); it != intervals.end(); ) {
            auto next = std::next(it);
            if (next != intervals.end() && next->start <= it->end + 1) {
                it->end = std::max(it->end, next->end);
                intervals.erase(next);
            }
            else {
                ++it;
            }
        }
    }
};
/**
 * @brief Structure representing a specific piece of an SDU for the MAC PDU.
 */
// TODO: sn should use modular arithmetic (limited to 2048 for 12-bit SN or 262143 for 18-bit)
struct PendingSegment {
    uint32_t sn = 0;
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t totalLength = 0;
    inet::Packet *ptr = nullptr;
    bool isValid = false;
};


class RlcSduSlidingWindowTransmissionBuffer : public omnetpp::cObject
{
  public:
    RlcSduSlidingWindowTransmissionBuffer(uint32_t windowSize, const std::string &name);
    ~RlcSduSlidingWindowTransmissionBuffer() override;

    /**
     * @brief Add a new SDU from the upper layer.
     * Assigns SN = txNext_ and increments txNext_.
     */
    uint32_t addSdu(uint32_t length, inet::Packet *sduPtr);

    /**
     * @brief Extract a single segment (or complete SDU) for the next transmission.
     * @param grantSize Total bytes available in the MAC PDU.
     * @return A PendingSegment containing data for at most one SDU.
     */
    PendingSegment getSegmentForGrant(uint32_t grantSize);

    /**
     * @brief Peek the start offset of the segment the next getSegmentForGrant() would
     * carve, without consuming it. Used to size the NR-SO PDU header before the carve
     * (a non-zero start means a continuation segment that needs the SO field).
     * @return true and sets outStart if a pending new-data segment exists.
     */
    bool peekNextSegmentStart(uint32_t &outStart) const;

    /**
     * @brief Process an RLC Status PDU (ACK/NACK).
     */
    std::set<uint32_t> handleAck(uint32_t sn, uint32_t start, uint32_t end,
                                 unsigned int pollSn, bool &restartPoll);

    /**
     * @brief Retrieve a segment for retransmission and update tracking.
     * @param sn  Retransmission task SN.
     * @param start  Segment start byte.
     * @param end  Segment end byte.
     * @param grantSize  Current MAC grant size.
     * @return Segment for retransmission if successful.
     */
    PendingSegment getRetransmissionSegment(uint32_t sn, uint32_t start,
                                            uint32_t end, uint32_t grantSize);

    /** @brief Total bytes not yet transmitted. */
    int getTotalPendingBytes() const;

    bool hasUnacknowledgedData() const { return txNext_ > txNextAck_; }
    bool windowFull() const { return txNext_ >= txNextAck_ + amWindowSize_; }

    bool isInRtxRange(uint32_t sn) const {
        return hasTransmitted_ && sn >= txNextAck_ && sn <= highestSnTransmitted_;
    }

    bool getSduData(uint32_t sn, inet::Packet *&outPtr, uint32_t &outTotalLen) {
        auto it = txBuffer_.find(sn);
        if (it == txBuffer_.end())
            return false;
        outPtr = it->second.sduPointer;
        outTotalLen = it->second.totalLength;
        return true;
    }

    bool isFullyAcknowledged(uint32_t sn) {
        auto it = txBuffer_.find(sn);
        if (it == txBuffer_.end())
            return false;
        return it->second.isFullyAcked();
    }

    uint32_t getTxNext() const { return txNext_; }
    uint32_t getTxNextAck() const { return txNextAck_; }
    uint32_t getCurrentWindowSize() const { return txNext_ - txNextAck_; }
    uint32_t getHighestSnTransmitted() const { return highestSnTransmitted_; }

  protected:
    std::map<uint32_t, SduTxState> txBuffer_;
    uint32_t amWindowSize_;
    std::string name_;
    bool hasTransmitted_ = false;
    uint32_t highestSnTransmitted_ = 0;

    // RLC AM state variables
    uint32_t txNext_ = 0;     // Next SN to assign to a new SDU
    uint32_t txNextAck_ = 0;  // Smallest SN for which ACK is pending
};

} // namespace simu5g

#endif // STACK_RLC_AM_BUFFER_RLCSDUSLIDINGWINDOWTRANSMISSIONBUFFER_H_
