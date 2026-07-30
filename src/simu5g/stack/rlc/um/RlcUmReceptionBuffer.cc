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

#include "RlcUmReceptionBuffer.h"

namespace simu5g {

using namespace inet;

RlcUmReceptionBuffer::RlcUmReceptionBuffer(uint32_t snBits ) {

    snModulus = (1 << snBits);
    UM_Window_Size = (1 << (snBits - 1)); // Half the SN space


}

RlcUmReceptionBuffer::~RlcUmReceptionBuffer() {
    clearBuffer();
}
bool RlcUmReceptionBuffer::handleSegment(uint32_t sn, uint32_t totalLen, uint32_t start, uint32_t end, Packet* ptr) {
    // 5.2.2.2.2: Basic Window Check
    //
    // Condition: (RX_Next_Highest – UM_Window_Size) <= SN < RX_Next_Reassembly
    uint32_t lowerBound = (RX_Next_Highest + snModulus - UM_Window_Size) % snModulus;
    bool isDiscardable = false;

    if (lowerBound <= RX_Next_Reassembly) {
        // Range does not wrap around
        isDiscardable = (sn >= lowerBound && sn < RX_Next_Reassembly);
    } else {
        // Range wraps around
        isDiscardable = (sn >= lowerBound || sn < RX_Next_Reassembly);
    }

    // At init: lowerBound=2048, RX_Next_Reassembly=0.
    // For SN=0: (0 >= 2048 || 0 < 0) is FALSE. SN=0 is NOT discarded.

    if (isDiscardable) {
        EV << "[RLC UM RX] Discarding SN=" << sn << " (Already Reassembled)" << std::endl;
        delete ptr;
        return false;
    }
    // Place in buffer
    auto it = sduBuffer.find(sn);
    if (it == sduBuffer.end()) {
        it = sduBuffer.emplace(sn, SduReassemblyState(totalLen, ptr)).first;
    } else {
        //TODO: remove when implementation changes to a proper reassembly buffer of bytes
        //A fragment  is already present. Since we actually send the complete SDU in all packets not only fragments
        //we need to delete the previous one. Once it is fully completed the last one is deleted when passing up
        if(it->second.sduPointer) {
            delete it->second.sduPointer;
            it->second.sduPointer=nullptr;

        }
        it->second.sduPointer=ptr;

    }

    auto result = it->second.addSegment(start, end);
    bool isNowComplete = result.second;

    // 5.2.2.2.3: Actions when placed in buffer
    if (isNowComplete) {
        //We need to point to null, because it will be delivered and may be freed
        it->second.sduPointer=nullptr;
        if (sn == RX_Next_Reassembly) {
            updateNextReassembly();
        }
    } else if (!isWithinWindow(sn)) {
        // SN=0 is NOT within window [2048, 0) because 0 is not < 0.
        // This triggers the window advance.
        RX_Next_Highest = (sn + 1) % snModulus;
        discardOutsideWindow();

        if (!isWithinWindow(RX_Next_Reassembly)) {
            setNextReassemblyToFirstInWindow();
        }
    }

    return isNowComplete;

}
void RlcUmReceptionBuffer::discardOutsideWindow() {
    for (auto it = sduBuffer.begin(); it != sduBuffer.end(); ) {
        if (!isWithinWindow(it->first)) {
            if (it->second.sduPointer) {
                delete it->second.sduPointer;
                it->second.sduPointer=nullptr;
            }
            it = sduBuffer.erase(it);
        } else {
            ++it;
        }
    }
}
bool RlcUmReceptionBuffer::stopTimer(bool isTimerRunning ) {
    bool shouldStop = false;
    if (isTimerRunning) {
        // Check stopping conditions from 5.2.2.2.3
        bool cond1 = !isSnLessThan(RX_Next_Reassembly, RX_Timer_Trigger); // RX_Timer_Trigger <= RX_Next_Reassembly
        bool cond2 = (!isWithinWindow(RX_Timer_Trigger) && RX_Timer_Trigger != RX_Next_Highest);

        bool cond3 = false;
        uint32_t nextAfterReas = (RX_Next_Reassembly + 1) % snModulus;
        if (RX_Next_Highest == nextAfterReas) {
            auto it = sduBuffer.find(RX_Next_Reassembly);
            if (it != sduBuffer.end() && !it->second.hasMissingByteSegmentBeforeLast()) {
                cond3 = true;
            }
        }

        if (cond1 || cond2 || cond3) {

            shouldStop = true;
        }
    }
    return shouldStop;
}
bool RlcUmReceptionBuffer::startTimer( )  {

    // Check starting conditions
    bool startCond1 = isSnLessThan(RX_Next_Reassembly + 1, RX_Next_Highest);
    bool startCond2 = false;
    if (RX_Next_Highest == (RX_Next_Reassembly + 1) % snModulus) {
        auto it = sduBuffer.find(RX_Next_Reassembly);
        if (it != sduBuffer.end() && it->second.hasMissingByteSegmentBeforeLast()) {
            startCond2 = true;
        }
    }

    if (startCond1 || startCond2) {

        RX_Timer_Trigger = RX_Next_Highest;
        return true;
    } else {
        return false;
    }

}
void RlcUmReceptionBuffer::onTimerExpiry() {
    // Update RX_Next_Reassembly to first SN >= RX_Timer_Trigger not reassembled
    RX_Next_Reassembly = RX_Timer_Trigger;
    updateNextReassembly();

    // Discard all segments with SN < updated RX_Next_Reassembly
    for (auto it = sduBuffer.begin(); it != sduBuffer.end(); ) {
        if (isSnLessThan(it->first, RX_Next_Reassembly)) {
            if (it->second.sduPointer) {
                delete it->second.sduPointer;
                it->second.sduPointer=nullptr;
            }
            it = sduBuffer.erase(it);
        } else {
            ++it;
        }
    }


}
} /* namespace simu5g */
