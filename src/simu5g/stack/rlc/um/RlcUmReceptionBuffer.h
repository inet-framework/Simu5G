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

#ifndef STACK_RLC_UM_RLCUMRECEPTIONBUFFER_H_
#define STACK_RLC_UM_RLCUMRECEPTIONBUFFER_H_

#include <omnetpp.h>
#include "simu5g/stack/rlc/am/RlcSduSlidingWindowReceptionBuffer.h"

namespace simu5g {

class RlcUmReceptionBuffer {
protected:
    std::map<uint32_t, SduReassemblyState> sduBuffer;

    // State Variables (Standard 7.1)
    uint32_t RX_Next_Reassembly = 0;
    uint32_t RX_Next_Highest = 0;
    uint32_t RX_Timer_Trigger = 0;

    uint32_t UM_Window_Size;
    uint32_t snModulus;
    bool isTimerRunning = false;
public:
    RlcUmReceptionBuffer(uint32_t snBits );
    virtual ~RlcUmReceptionBuffer();


    /**
     * @brief Check if SN falls within the reassembly window.
     * Standard: (RX_Next_Highest – UM_Window_Size) <= SN < RX_Next_Highest
     */
    virtual bool isWithinWindow(uint32_t sn) const {
        uint32_t lowerBound = (RX_Next_Highest + snModulus - UM_Window_Size) % snModulus;

        if (lowerBound < RX_Next_Highest) {
            return (sn >= lowerBound && sn < RX_Next_Highest);
        } else if (lowerBound > RX_Next_Highest) {
            return (sn >= lowerBound || sn < RX_Next_Highest);
        } else {
            // Initialization case (0 == 0): SN 0 is NOT < 0.
            return false;
        }
    }

    virtual bool isEmpty() const {
        return sduBuffer.empty();
    }


    virtual void reset() {
        RX_Next_Reassembly = 0;
        RX_Next_Highest = 0;
        RX_Timer_Trigger = 0;
    }
    /**
     * @brief Processes an incoming UMD PDU segment.
     * returns true is the SDU is complete
     */
    virtual bool handleSegment(uint32_t sn, uint32_t totalLen, uint32_t start, uint32_t end, inet::Packet* ptr);

    virtual void onTimerExpiry();


    virtual bool isSnLessThan(uint32_t sn1, uint32_t sn2) const {
        // Modular comparison logic
        uint32_t diff = (sn2 + snModulus - sn1) % snModulus;
        return (diff > 0 && diff < UM_Window_Size);
    }

    virtual void updateNextReassembly() {
        // Advance past consecutive fully-reassembled (already delivered) SDUs, erasing
        // their now-empty bookkeeping entries as we go. Without the erase the map keeps
        // every delivered SN forever; once it spans the whole SN space this loop would
        // find a complete entry at every SN and never terminate (an infinite loop inside
        // a single event). Duplicate detection is by SN window (see handleSegment), not
        // by entry presence, so dropping delivered entries below RX_Next_Reassembly is safe.
        while (sduBuffer.count(RX_Next_Reassembly) && sduBuffer.at(RX_Next_Reassembly).isComplete) {
            sduBuffer.erase(RX_Next_Reassembly);
            RX_Next_Reassembly = (RX_Next_Reassembly + 1) % snModulus;
        }
    }

    virtual void setNextReassemblyToFirstInWindow() {
        uint32_t lowerBound = (RX_Next_Highest + snModulus - UM_Window_Size) % snModulus;
        RX_Next_Reassembly = lowerBound;

        updateNextReassembly();
    }

    virtual void discardOutsideWindow();

    virtual bool stopTimer(bool isTimerRunning );
    virtual bool startTimer() ;
    /**
     * @brief Clears the reception buffer and deletes all stored packet pointers.
     */
    virtual void clearBuffer() {
        for (auto& pair : sduBuffer) {
            if (pair.second.sduPointer) {
                delete pair.second.sduPointer;
                pair.second.sduPointer = nullptr;
            }
        }
        sduBuffer.clear();
    }


};

} /* namespace simu5g */

#endif /* STACK_RLC_UM_RLCUMRECEPTIONBUFFER_H_ */
