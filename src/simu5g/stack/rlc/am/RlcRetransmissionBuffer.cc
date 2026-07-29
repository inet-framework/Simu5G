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

#include "RlcRetransmissionBuffer.h"

namespace simu5g {

RlcRetransmissionBuffer::RlcRetransmissionBuffer(uint32_t threshold)
    : maxRetxThreshold_(threshold)
{
}

RlcRetransmissionBuffer::~RlcRetransmissionBuffer()
{
}


bool RlcRetransmissionBuffer::addNack(uint32_t sn, bool isWhole, uint32_t start, uint32_t end)
{
    RetxTask task{sn, start, end, isWhole};
    bool alreadyPending = (pendingRetx_.find(task) != pendingRetx_.end());

    if (retxCounters_.find(sn) == retxCounters_.end()) {
        // Considered for retransmission for the first time: RETX_COUNT starts at zero.
        retxCounters_[sn].retxCount = 0;
    }
    else if (!alreadyPending && !retxCounters_[sn].incrementedInCurrentRound) {
        retxCounters_[sn].retxCount++;
        retxCounters_[sn].incrementedInCurrentRound = true;

        if (retxCounters_[sn].retxCount >= maxRetxThreshold_)
            return false;
    }

    pendingRetx_.insert(task);
    return true;
}
void RlcRetransmissionBuffer::clearSn(uint32_t sn)
{
    retxCounters_.erase(sn);
    for (auto it = pendingRetx_.begin(); it != pendingRetx_.end(); ) {
        if (it->sn == sn)
            it = pendingRetx_.erase(it);
        else
            ++it;
    }
}
uint64_t RlcRetransmissionBuffer::getRetxPendingBytes()
{
    uint64_t total = 0;
    for (const auto &task : pendingRetx_)
        total += (task.soEnd - task.soStart + 1);
    return total;
}

} // namespace simu5g
