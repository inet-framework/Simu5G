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

#ifndef STACK_RLC_AM_BUFFER_RLCRETRANSMISSIONBUFFER_H_
#define STACK_RLC_AM_BUFFER_RLCRETRANSMISSIONBUFFER_H_

#include <omnetpp.h>

#include <map>
#include <set>

namespace simu5g {

/**
 * @brief Represents a specific segment or a whole data unit pending for
 * retransmission. The unit the sequence number refers to is the mode's ARQ unit:
 * an SDU for the NR AM entity (TS 38.322), an AMD PDU for the LTE one (TS 36.322).
 */
struct RetxTask {
    uint32_t sn;
    uint32_t soStart;
    uint32_t soEnd;
    bool isWholeUnit;

    bool operator<(const RetxTask &other) const {
        if (sn != other.sn) return sn < other.sn;
        if (soStart != other.soStart) return soStart < other.soStart;
        return soEnd < other.soEnd;
    }
};

class RlcRetransmissionBuffer
{
  public:
    RlcRetransmissionBuffer(uint32_t threshold);
    virtual ~RlcRetransmissionBuffer();

    /**
     * @brief Start a new retransmission round.
     *
     * RETX_COUNT is incremented at most once per unit per round (TS 38.322 5.3.2 /
     * TS 36.322 5.2.1). A round is opened by an arriving STATUS PDU, whose NACKs may
     * name the same unit
     * several times, and also by a t-PollRetransmit expiry, which likewise triggers a
     * retransmission. Both callers must open a round, otherwise the per-round flags
     * stay set and the counter stops advancing.
     */
    virtual void beginRetxRound() {
        for (auto &[sn, state] : retxCounters_)
            state.incrementedInCurrentRound = false;
    }

    /**
     * @brief Add a NACKed SDU or segment to the retransmission pool.
     *
     * Returns false if this unit has reached maxRetxThreshold retransmissions, in
     * which case it is not added and the caller must declare a radio link failure.
     */
    virtual bool addNack(uint32_t sn, bool isWhole, uint32_t start, uint32_t end);

    /** @brief Returns the next task to be retransmitted. */
    virtual bool getNextRetxTask(RetxTask &outTask) {
        if (pendingRetx_.empty())
            return false;
        outTask = *pendingRetx_.begin();
        return true;
    }

    /** @brief Returns the total pending payload bytes for retransmissions. */
    virtual uint64_t getRetxPendingBytes();

    /** @brief The segments and whole SDUs currently awaiting retransmission. */
    const std::set<RetxTask>& getPendingRetx() const { return pendingRetx_; }

    /** @brief Remove a task once retransmission is submitted to lower layer. */
    void markRetransmitted(const RetxTask &task) { pendingRetx_.erase(task); }

    /** @brief Clear retx state for a sequence number (e.g., when finally ACKed). */
    virtual void clearSn(uint32_t sn);

  protected:
    struct RetxState {
        uint32_t retxCount = 0;
        bool incrementedInCurrentRound = false;
    };

    std::map<uint32_t, RetxState> retxCounters_;
    std::set<RetxTask> pendingRetx_;
    uint32_t maxRetxThreshold_;
};

} // namespace simu5g

#endif // STACK_RLC_AM_BUFFER_RLCRETRANSMISSIONBUFFER_H_
