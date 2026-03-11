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

#include "RlcSduRetransmissionBuffer.h"

namespace simu5g {

RlcSduRetransmissionBuffer::RlcSduRetransmissionBuffer(uint32_t threshold) : maxRetxThreshold(threshold) {

}

RlcSduRetransmissionBuffer::~RlcSduRetransmissionBuffer() {
}


bool RlcSduRetransmissionBuffer::addNack(uint32_t sn, bool isWhole, uint32_t start, uint32_t end ) {
    RetxTask task{sn, start, end, isWhole};

    // Check if this specific segment is already pending
    bool alreadyPending = (pendingRetx.find(task) != pendingRetx.end());

    if (sduRetxCounters.find(sn) == sduRetxCounters.end()) {
        // First time this SDU is considered for retransmission
        sduRetxCounters[sn].retxCount = 0;
    } else if (!alreadyPending && !sduRetxCounters[sn].incrementedInCurrentStatusPdu) {
        // Increment RETX_COUNT if not already pending and not already incremented for this PDU
        sduRetxCounters[sn].retxCount++;
        sduRetxCounters[sn].incrementedInCurrentStatusPdu = true;

        if (sduRetxCounters[sn].retxCount >= maxRetxThreshold) {
            std::cout << "[RLC RETX] CRITICAL: Max retransmissions reached for SN=" << sn << std::endl;
            return false;
        }
    }

    pendingRetx.insert(task);
    return true;
}
void RlcSduRetransmissionBuffer::clearSdu(uint32_t sn) {
    sduRetxCounters.erase(sn);
    for (auto it = pendingRetx.begin(); it != pendingRetx.end(); ) {
        if (it->sn == sn) it = pendingRetx.erase(it);
        else ++it;
    }
}
uint64_t RlcSduRetransmissionBuffer::getRetxPendingBytes() {
    uint64_t total = 0;
    for (const auto& task : pendingRetx) {

        total += (task.soEnd - task.soStart + 1);

    }

    return total;
}




} /* namespace simu5g */
