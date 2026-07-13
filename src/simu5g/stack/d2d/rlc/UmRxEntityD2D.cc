//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/d2d/rlc/UmRxEntityD2D.h"

namespace simu5g {

Define_Module(UmRxEntityD2D);

// NOTE: no static registerSignal() calls in this translation unit -- the four
// D2D statistic signals emitted by this module are registered in UmRxEntity.cc
// (global signal-ID order stability, sz fingerprint) and inherited as
// protected statics.

using namespace inet;

void UmRxEntityD2D::onFirstPduEnqueued(unsigned int pduSno)
{
    if (!init_ && isD2DMultiConnection()) {
        // for D2D multicast connections, the first received PDU must be considered as the first valid PDU
        rxWindowDesc_.clear(pduSno);
        // setting the window size to 1 allows the entity to deliver immediately out-of-sequence SDU,
        // since reordering is not applicable for D2D multicast communications
        rxWindowDesc_.windowSize_ = 1;
        init_ = true;
    }
}

bool UmRxEntityD2D::consumeReassemblyReset(unsigned int pduSno)
{
    if (!resetFlag_)
        return false;

    // by doing this, the arrived PDU and its first extracted SDU will be considered in order.
    // This helps to retrieve the synchronization between SNs at the tx and rx after a mode switch.
    lastPduReassembled_ = pduSno - 1;
    resetFlag_ = false;
    return true;
}

void UmRxEntityD2D::emitPduStats(cModule *ue, Direction dir, double tputSample, simtime_t creationTime)
{
    if (dir == D2D || dir == D2D_MULTI) { // UE in DM
        if (ue != nullptr) {
            ue->emit(rlcPduThroughputD2DSignal_, tputSample);
            ue->emit(rlcPduDelayD2DSignal_, (NOW - creationTime).dbl());
        }
    }
    else { // UE in IM
        UmRxEntity::emitPduStats(ue, dir, tputSample, creationTime);
    }
}

void UmRxEntityD2D::emitSduStats(cModule *ue, Direction dir, double tputSample, simtime_t creationTime)
{
    if (dir == D2D || dir == D2D_MULTI) { // UE in DM
        if (ue != nullptr) {
            ue->emit(rlcThroughputD2DSignal_, tputSample);
            ue->emit(rlcDelayD2DSignal_, (NOW - creationTime).dbl());
        }
    }
    else { // UE in IM
        UmRxEntity::emitSduStats(ue, dir, tputSample, creationTime);
    }
}

void UmRxEntityD2D::rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer)
{
    Enter_Method_Silent("rlcHandleD2DModeSwitch()");

    if (oldConnection) {
        if (getNodeTypeById(ownerNodeId_) == UE && oldMode == IM) {
            EV << NOW << " UmRxEntityD2D::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer) {

            EV << NOW << " UmRxEntityD2D::rlcHandleD2DModeSwitch - clear RX buffer of the RLC entity associated with the old mode" << endl;
            for (unsigned int i = 0; i < rxWindowDesc_.windowSize_; i++) {
                // try to reassemble
                reassemble(i);
            }

            // clear the buffer
            pduBuffer_.clear();

            for (auto && i : received_) {
                i = false;
            }

            clearBufferedSdu();

            // stop the timer
            if (t_reordering_.busy())
                t_reordering_.stop();
        }
    }
    else {
        EV << NOW << " UmRxEntityD2D::rlcHandleD2DModeSwitch - handle numbering of the RLC entity associated with the newly selected mode" << endl;

        // reset sequence numbering
        rxWindowDesc_.clear();

        resetFlag_ = true;
    }
}

} //namespace
