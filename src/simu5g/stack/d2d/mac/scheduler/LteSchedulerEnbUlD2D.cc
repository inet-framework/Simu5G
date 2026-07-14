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

#include "simu5g/stack/d2d/mac/scheduler/LteSchedulerEnbUlD2D.h"
#include "simu5g/common/InitStages.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/harq/LteHarqBufferMirrorD2D.h"
#include "simu5g/stack/d2d/mac/scheduler/LteAllocatorBestFit.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(LteSchedulerEnbUlD2D);

void LteSchedulerEnbUlD2D::initialize(int stage)
{
    LteSchedulerEnbUl::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_AMC_SETUP)
        d2dMac_ = check_and_cast<ID2dMacEnb *>(mac_.get());
}

unsigned int LteSchedulerEnbUlD2D::scheduleAdditionalRetransmissions(GHz carrierFrequency, BandLimitVector *bandLim)
{
    unsigned int totalAllocatedBytes = 0;
    Direction dir = D2D;
    HarqBuffersMirrorD2D *harqBuffersMirrorD2D = d2dMac_->getHarqBuffersMirrorD2D(carrierFrequency);
    if (harqBuffersMirrorD2D != nullptr) {
        for (auto it_d2d = harqBuffersMirrorD2D->begin(); it_d2d != harqBuffersMirrorD2D->end(); ) {
            auto& [d2dPair, harqBufferMirror] = *it_d2d;
            MacNodeId senderId = d2dPair.first; // Transmitter
            MacNodeId destId = d2dPair.second;  // Receiver

            if (senderId == NODEID_NONE || !binder_->nodeExists(senderId)) {
                // UE has left the simulation - erase queue and continue
                it_d2d = harqBuffersMirrorD2D->erase(it_d2d);
                continue;
            }
            if (destId == NODEID_NONE || !binder_->nodeExists(destId)) {
                // UE has left the simulation - erase queue and continue
                it_d2d = harqBuffersMirrorD2D->erase(it_d2d);
                continue;
            }

            // get current Harq Process for nodeId
            unsigned char currentAcid = harqStatus_[carrierFrequency].at(senderId);

            // check whether the UE has a H-ARQ process waiting for retransmission. If not, skip UE.
            bool skip = true;
            unsigned char acid = (currentAcid + 2) % (harqBufferMirror->getProcesses());
            LteHarqProcessMirrorD2D *currentProcess = harqBufferMirror->getProcess(acid);
            std::vector<TxHarqPduStatus> procStatus = currentProcess->getProcessStatus();
            for (const auto& status : procStatus) {
                if (status == TXHARQ_PDU_BUFFERED) {
                    skip = false;
                    break;
                }
            }
            if (skip) {
                ++it_d2d;
                continue;
            }

            EV << NOW << " LteSchedulerEnbUlD2D::scheduleAdditionalRetransmissions - D2D UE: " << senderId << " Acid: " << (unsigned int)currentAcid << endl;

            const UserTxParams& txParams = mac_->getAmc()->computeTxParams(senderId, dir, carrierFrequency);
            unsigned int codewords = txParams.getLayers().size();
            unsigned int allocatedBytes = 0;

            for (Codeword cw = 0; (cw < MAX_CODEWORDS) && (codewords > 0); ++cw) {
                unsigned int rtxBytes = d2dRtxScheduling_.schedulePerAcidRtxD2D(destId, senderId, carrierFrequency, cw, acid, bandLim);
                if (rtxBytes > 0) {
                    --codewords;
                    allocatedBytes += rtxBytes;

                    mac_->signalProcessForRtx(senderId, carrierFrequency, D2D, false);
                }
            }
            EV << NOW << " LteSchedulerEnbUlD2D::scheduleAdditionalRetransmissions - D2D UE: " << senderId << " allocated bytes : " << allocatedBytes << endl;
            totalAllocatedBytes += allocatedBytes;
            ++it_d2d;
        }
    }
    return totalAllocatedBytes;
}

LteScheduler *LteSchedulerEnbUlD2D::getScheduler(SchedDiscipline discipline)
{
    if (discipline == ALLOCATOR_BESTFIT)
        return new LteAllocatorBestFit(binder_);
    return LteSchedulerEnbUl::getScheduler(discipline);
}

} //namespace
