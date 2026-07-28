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

#include "simu5g/stack/d2d/mac/scheduler/D2dRtxScheduling.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerEnbUl.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/harq/LteHarqBufferMirrorD2D.h"
#include "simu5g/stack/mac/allocator/LteAllocationModule.h"

namespace simu5g {

using namespace omnetpp;

unsigned int D2dRtxScheduling::schedulePerAcidRtxD2D(MacNodeId destId, MacNodeId senderId, GHz carrierFrequency, Codeword cw, unsigned char acid,
        std::vector<BandLimit> *bandLim, Remote antenna, bool limitBl)
{
    Direction dir = D2D;
    try {
        // apply the allowed-band restriction for this sender
        BandLimitVector tempBandLim;
        bandLim = scheduler_->applyAllowedBandLimits(senderId, dir, carrierFrequency, bandLim, tempBandLim);

        EV << NOW << "D2dRtxScheduling::schedulePerAcidRtxD2D - Node[" << scheduler_->mac_->getMacNodeId() << ", User[" << senderId << ", Codeword[ " << cw << "], ACID[" << (unsigned int)acid << "] " << endl;

        D2DPair pair(senderId, destId);

        // Get the current active HARQ process
        HarqBuffersMirrorD2D *harqBuffersMirrorD2D = check_and_cast<ID2dMacEnb *>(scheduler_->mac_.get())->getHarqBuffersMirrorD2D(carrierFrequency);
        EV << "\t the acid that should be considered is " << (unsigned int)acid << endl;

        LteHarqProcessMirrorD2D *currentProcess = harqBuffersMirrorD2D->at(pair)->getProcess(acid);
        if (currentProcess->getUnitStatus(cw) != TXHARQ_PDU_BUFFERED) {
            // exit if the current active HARQ process is not ready for retransmission
            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D User is on ACID " << (unsigned int)acid << " HARQ process is IDLE. No RTX scheduled ." << endl;
            return 0;
        }

        // serve the buffered PDU across the allowed bands and record the allocation
        bool served;
        unsigned int bytes = scheduler_->allocateRtxBytes(senderId, dir, D2D_SHORT_BSR, currentProcess->getPduLength(cw), carrierFrequency, cw, acid, bandLim, antenna, served);
        if (served)
            currentProcess->markSelected(cw);
        return bytes;
    }
    catch (std::exception& e) {
        throw cRuntimeError("Exception in D2dRtxScheduling::schedulePerAcidRtxD2D(): %s", e.what());
    }
    return 0;
}

} //namespace
