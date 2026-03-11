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

#ifndef STACK_RLC_AM_BUFFER_RLCSDURETRANSMISSIONBUFFER_H_
#define STACK_RLC_AM_BUFFER_RLCSDURETRANSMISSIONBUFFER_H_

#include <omnetpp.h>
#include <vector>
#include <set>
#include <algorithm>

namespace simu5g {


/**
 * @brief Represents a specific segment or a whole SDU pending for retransmission.
 */
struct RetxTask {
    uint32_t sn;
    uint32_t soStart;
    uint32_t soEnd;
    bool isWholeSdu;

    bool operator<(const RetxTask& other) const {
        if (sn != other.sn) return sn < other.sn;
        if (soStart != other.soStart) return soStart < other.soStart;
        return soEnd < other.soEnd;
    }
};



class RlcSduRetransmissionBuffer {
protected:
    struct RetxState {
        uint32_t retxCount = 0;
        bool incrementedInCurrentStatusPdu = false;
    };

    std::map<uint32_t, RetxState> sduRetxCounters;
    std::set<RetxTask> pendingRetx;
    uint32_t maxRetxThreshold;
public:
    RlcSduRetransmissionBuffer(uint32_t threshold);
    virtual ~RlcSduRetransmissionBuffer();


    /**
     * @brief Prepare the buffer for a new Status PDU processing.
     * Resets the per-Status PDU increment flag.
     */
    void beginStatusPduProcessing() {
        for (auto& pair : sduRetxCounters) {
            pair.second.incrementedInCurrentStatusPdu = false;
        }
    }

    /**
     * @brief Adds a NACKed SDU or segment to the retransmission pool.
     * Implements RETX_COUNT logic according to the standard.
     */
    bool addNack(uint32_t sn, bool isWhole, uint32_t start , uint32_t end ) ;

    /**
     * @brief Returns the next task to be retransmitted.
     */
    bool getNextRetxTask(RetxTask& outTask) {
        if (pendingRetx.empty()) return false;
        outTask = *pendingRetx.begin();
        return true;
    }
    /**
     * @brief Returns the total pending bytes specifically for retransmissions.
     * 
     */
    uint64_t getRetxPendingBytes() ;

    /**
     * @brief Removes a task once the retransmission is submitted to lower layer.
     */
    void markRetransmitted(const RetxTask& task) {
        pendingRetx.erase(task);
    }

    /**
     * @brief Clears retx state for an SDU (e.g., when it is finally ACKed).
     */
    void clearSdu(uint32_t sn) ; 

};
} /* namespace simu5g */

#endif /* STACK_RLC_AM_BUFFER_RLCSDURETRANSMISSIONBUFFER_H_ */
