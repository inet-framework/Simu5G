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

#include "simu5g/stack/d2d/mac/LteMacUeD2D.h"

#include "simu5g/stack/mac/buffer/LteMacBuffer.h"
#include "simu5g/stack/mac/buffer/LteMacQueue.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerUeUl.h"

namespace simu5g {

Define_Module(LteMacUeD2D);

using namespace inet;

void LteMacUeD2D::purgeRxHarqBuffers()
{
    unsigned int purged = 0;
    // purge from corrupted PDUs all Rx H-HARQ buffers
    for (auto& [carrierFreq, harqRxMap] : harqRxBuffers_) {
        for (auto& [nodeId, rxBuffer] : harqRxMap) {
            // purge corrupted PDUs only if this buffer is for a DL transmission. Otherwise, if you
            // purge PDUs for D2D communication, also "mirror" buffers will be purged
            if (nodeId == cellId_)
                purged += rxBuffer->purgeCorruptedPdus();
        }
    }
    EV << NOW << " LteMacUeD2D::purgeRxHarqBuffers Purged " << purged << " PDUs" << endl;
}

} //namespace
