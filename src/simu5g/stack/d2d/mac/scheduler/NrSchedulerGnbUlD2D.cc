//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/d2d/mac/scheduler/NrSchedulerGnbUlD2D.h"
#include "simu5g/common/InitStages.h"
#include "simu5g/stack/mac/NrMacGnb.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/harq/LteHarqBufferMirrorD2D.h"
#include "simu5g/stack/d2d/mac/scheduler/LteAllocatorBestFit.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(NrSchedulerGnbUlD2D);

void NrSchedulerGnbUlD2D::initialize(int stage)
{
    NrSchedulerGnbUl::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_AMC_SETUP)
        d2dMac_ = check_and_cast<ID2dMacEnb *>(mac_.get());
}

unsigned int NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions(GHz carrierFrequency, BandLimitVector *bandLim)
{
    unsigned int totalAllocatedBytes = 0;
    // --- START Schedule D2D retransmissions --- //
    Direction dir = D2D;
    HarqBuffersMirrorD2D *harqBuffersMirrorD2D = d2dMac_->getHarqBuffersMirrorD2D(carrierFrequency);
    if (harqBuffersMirrorD2D != nullptr) {
        for (auto it_d2d = harqBuffersMirrorD2D->begin(); it_d2d != harqBuffersMirrorD2D->end(); ) {
            auto& [d2dPair, currHarq] = *it_d2d;
            // get current nodeIDs
            MacNodeId senderId = d2dPair.first; // Transmitter
            MacNodeId destId = d2dPair.second;  // Receiver

            if (senderId == NODEID_NONE || !binder_->nodeExists(senderId)) {
                // UE has left the simulation - erase queue and continue
                harqBuffersMirrorD2D->erase(it_d2d++);
                continue;
            }
            if (destId == NODEID_NONE || !binder_->nodeExists(destId)) {
                // UE has left the simulation - erase queue and continue
                harqBuffersMirrorD2D->erase(it_d2d++);
                continue;
            }

            // Get user transmission parameters
            const UserTxParams& txParams = mac_->getAmc()->computeTxParams(senderId, dir, carrierFrequency);// get the user info

            unsigned int codewords = txParams.getLayers().size();// get the number of available codewords
            unsigned int allocatedBytes = 0;

            // TODO handle the codewords join case (size of(cw0+cw1) < currentTBS && currentLayers ==1)

            EV << NOW << " NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions D2D TX UE: " << senderId << " - RX UE: " << destId << endl;

            // get the number of HARQ processes
            unsigned int maxProcesses = currHarq->getProcesses();

            for (unsigned int process = 0; process < maxProcesses; ++process) {
                // for each HARQ process
                LteHarqProcessMirrorD2D *currProc = currHarq->getProcess(process);

                if (allocatedCws_[senderId] == codewords)
                    break;

                for (Codeword cw = 0; (cw < MAX_CODEWORDS) && (codewords > 0); ++cw) {
                    EV << NOW << " NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions process " << process << endl;
                    EV << NOW << " NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions ------- CODEWORD " << cw << endl;

                    // skip processes which are not in RTX status
                    if (currProc->getUnitStatus(cw) != TXHARQ_PDU_BUFFERED) {
                        EV << NOW << " NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions D2D UE: " << senderId << " detected Acid: " << process << " in status " << currProc->getUnitStatus(cw) << endl;
                        continue;
                    }

                    // FIXME PERFORMANCE: check for RTX status before calling rtxAcid

                    // perform a retransmission on available codewords for the selected acid
                    unsigned int rtxBytes = d2dRtxScheduling_.schedulePerAcidRtxD2D(destId, senderId, carrierFrequency, cw, process, bandLim);
                    if (rtxBytes > 0) {
                        --codewords;
                        allocatedBytes += rtxBytes;

                        mac_->signalProcessForRtx(senderId, carrierFrequency, D2D, false);
                    }
                }
                EV << NOW << " NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions - D2D UE: " << senderId << " allocated bytes : " << allocatedBytes << endl;
            }
            totalAllocatedBytes += allocatedBytes;
            ++it_d2d;
        }
    }
    // --- END Schedule D2D retransmissions --- //
    return totalAllocatedBytes;
}

LteScheduler *NrSchedulerGnbUlD2D::getScheduler(SchedDiscipline discipline)
{
    if (discipline == ALLOCATOR_BESTFIT)
        return new LteAllocatorBestFit(binder_);
    return NrSchedulerGnbUl::getScheduler(discipline);
}

} //namespace
