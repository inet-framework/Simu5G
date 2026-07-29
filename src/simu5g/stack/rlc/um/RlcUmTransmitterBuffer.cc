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

#include "RlcUmTransmitterBuffer.h"

namespace simu5g {

RlcUmTransmitterBuffer::RlcUmTransmitterBuffer(uint32_t snBits) {
    snModulus = (1 << snBits);

}

RlcUmTransmitterBuffer::~RlcUmTransmitterBuffer() {
}
PendingSegmentUM RlcUmTransmitterBuffer::getSegmentForGrant(uint32_t grantSize) {
    PendingSegmentUM seg;
    if (txBuffer.empty() || grantSize == 0) return seg;

    auto& currentSdu = txBuffer.front();

    // 5.2.2.1.1: If it's a segment, it gets TX_Next
    if (currentSdu.getNextSegment(grantSize, seg.start, seg.end)) {
        // A whole SDU that fits in one PDU (covers [0, totalLength-1] on its first and
        // only transmission) is a complete SDU. Per TS 38.322 it carries no SN and does
        // not advance TX_Next (which numbers segmented SDUs only).
        bool wholeInOnePdu = (seg.start == 0 && seg.end == currentSdu.totalLength - 1);
        if (wholeInOnePdu) {
            seg.ptr = currentSdu.sduPointer;
            seg.totalLength = currentSdu.totalLength;
            seg.isValid = true;
            seg.isFull = true;
            currentSdu.markTransmitted(seg.start, seg.end);
            txBuffer.pop_front();
            return seg;
        }

        seg.sn = TX_Next;
        seg.ptr = currentSdu.sduPointer;
        seg.totalLength = currentSdu.totalLength;
        seg.isValid = true;

        currentSdu.markTransmitted(seg.start, seg.end);

        // 5.2.2.1.1: If segment maps to the last byte, increment TX_Next
        if (seg.end == currentSdu.totalLength - 1) {

            seg.isLastSegment = true;
            TX_Next = (TX_Next + 1) % snModulus;

            // Once fully transmitted, UM removes it from buffer (no retransmissions)
            txBuffer.pop_front();
        }
    } else {
        //This is a full SDU
        seg.ptr = currentSdu.sduPointer;
        seg.totalLength = currentSdu.totalLength;
        seg.isValid = true;
        seg.isFull=true;
        currentSdu.markTransmitted(seg.start, seg.end);
        ASSERT(seg.start==0 && seg.end==(currentSdu.totalLength - 1));
        // Once fully transmitted, UM removes it from buffer (no retransmissions)
        txBuffer.pop_front();

    }

    return seg;
}

} /* namespace simu5g */
