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
#include "simu5g/stack/mac/buffer/harq_d2d/LteHarqBufferMirrorD2D.h"
#include "simu5g/stack/mac/allocator/LteAllocationModule.h"

namespace simu5g {

using namespace omnetpp;

unsigned int D2dRtxScheduling::schedulePerAcidRtxD2D(MacNodeId destId, MacNodeId senderId, GHz carrierFrequency, Codeword cw, unsigned char acid,
        std::vector<BandLimit> *bandLim, Remote antenna, bool limitBl)
{
    Direction dir = D2D;
    try {
        const UserTxParams& txParams = scheduler_->mac_->getAmc()->computeTxParams(senderId, dir, carrierFrequency);    // get the user info
        const std::set<Band>& allowedBands = txParams.readBands();
        BandLimitVector tempBandLim;
        std::string bands_msg = "BAND_LIMIT_SPECIFIED";
        if (bandLim == nullptr) {
            // Create a vector of band limit using all bands
            // FIXME: bandLim is never deleted

            unsigned int numBands = scheduler_->mac_->getCellInfo()->getNumBands();
            // for each band of the band vector provided
            for (unsigned int i = 0; i < numBands; i++) {
                BandLimit elem;
                // copy the band
                elem.band_ = Band(i);
                EV << "Putting band " << i << endl;
                for (unsigned int j = 0; j < MAX_CODEWORDS; j++) {
                    if (allowedBands.find(elem.band_) != allowedBands.end()) {
                        EV << "\t" << i << " " << "yes" << endl;
                        elem.limit_[j] = -1;
                    }
                    else {
                        EV << "\t" << i << " " << "no" << endl;
                        elem.limit_[j] = -2;
                    }
                }
                tempBandLim.push_back(elem);
            }
            bandLim = &tempBandLim;
        }
        else {
            unsigned int numBands = scheduler_->mac_->getCellInfo()->getNumBands();
            // for each band of the band vector provided
            for (unsigned int i = 0; i < numBands; i++) {
                BandLimit& elem = bandLim->at(i);
                for (unsigned int j = 0; j < MAX_CODEWORDS; j++) {
                    if (elem.limit_[j] == -2)
                        continue;

                    if (allowedBands.find(elem.band_) != allowedBands.end()) {
                        EV << "\t" << i << " " << "yes" << endl;
                        elem.limit_[j] = -1;
                    }
                    else {
                        EV << "\t" << i << " " << "no" << endl;
                        elem.limit_[j] = -2;
                    }
                }
            }
        }

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

        Codeword allocatedCw = 0;
        //search for already allocated codeword
        //create "mirror" scList ID for other codeword than current
        std::pair<MacCid, Codeword> scListMirrorId = {MacCid(senderId, D2D_SHORT_BSR), MAX_CODEWORDS - cw - 1};
        if (scheduler_->scheduleList_.find(carrierFrequency) != scheduler_->scheduleList_.end()) {
            if (scheduler_->scheduleList_[carrierFrequency].find(scListMirrorId) != scheduler_->scheduleList_[carrierFrequency].end()) {
                allocatedCw = MAX_CODEWORDS - cw - 1;
            }
        }
        // get current process buffered PDU byte length
        unsigned int bytes = currentProcess->getPduLength(cw);
        // bytes to serve
        unsigned int toServe = bytes;
        // blocks to allocate for each band
        std::vector<unsigned int> assignedBlocks;
        // bytes which blocks from the preceding vector are supposed to satisfy
        std::vector<unsigned int> assignedBytes;

        // end loop signal [same as bytes>0, but more secure]
        bool finish = false;
        // for each band
        unsigned int size = bandLim->size();
        for (unsigned int i = 0; (i < size) && (!finish); ++i) {
            // save the band and the relative limit
            Band b = bandLim->at(i).band_;
            int limit = bandLim->at(i).limit_.at(cw);

            // TODO add support for multi CW
            //unsigned int bandAvailableBytes = // if a codeword has been already scheduled for retransmission, limit available blocks to what's been allocated on that codeword
            //((allocatedCw == MAX_CODEWORDS) ? availableBytes(nodeId,antenna, b, cw) : mac_->getAmc()->blocks2bytes(nodeId, b, cw, allocator_->getBlocks(antenna,b,nodeId) , direction_));    // available space
            unsigned int bandAvailableBytes = scheduler_->availableBytes(senderId, antenna, b, cw, dir, carrierFrequency);

            // use the provided limit as cap for available bytes, if it is not set to unlimited
            if (limit >= 0)
                bandAvailableBytes = limit < (int)bandAvailableBytes ? limit : bandAvailableBytes;

            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D BAND " << b << endl;
            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D total bytes:" << bytes << " still to serve: " << toServe << " bytes" << endl;
            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D Available: " << bandAvailableBytes << " bytes" << endl;

            unsigned int servedBytes = 0;
            // there's no room on current band for serving the entire request
            if (bandAvailableBytes < toServe) {
                // record the amount of served bytes
                servedBytes = bandAvailableBytes;
                // the request can be fully satisfied
            }
            else {
                // record the amount of served bytes
                servedBytes = toServe;
                // signal end loop - all data have been serviced
                finish = true;
                EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D ALL DATA HAVE BEEN SERVICED" << endl;
            }
            unsigned int servedBlocks = (servedBytes == 0) ? 0 : 1;
            // update the bytes counter
            toServe -= servedBytes;
            // update the structures
            assignedBlocks.push_back(servedBlocks);
            assignedBytes.push_back(servedBytes);
        }

        if (toServe > 0) {
            // process couldn't be served - no sufficient space on available bands
            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D Unavailable space for serving node " << senderId << " ,HARQ Process " << (unsigned int)acid << " on codeword " << cw << endl;
            return 0;
        }
        else {
            // record the allocation
            unsigned int size = assignedBlocks.size();
            unsigned int cwAllocatedBlocks = 0;

            // create scList id for current cid/codeword
            std::pair<MacCid, Codeword> scListId = {MacCid(senderId, D2D_SHORT_BSR), cw};

            for (unsigned int i = 0; i < size; ++i) {
                // For each LB for which blocks have been allocated
                Band b = bandLim->at(i).band_;

                cwAllocatedBlocks += assignedBlocks.at(i);
                EV << "\t Cw->" << allocatedCw << "/" << MAX_CODEWORDS << endl;
                //! handle multi-codeword allocation
                if (allocatedCw != MAX_CODEWORDS) {
                    EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D - adding " << assignedBlocks.at(i) << " to band " << i << endl;
                    scheduler_->allocator_->addBlocks(antenna, b, senderId, assignedBlocks.at(i), assignedBytes.at(i));
                }
                //! TODO check if ok bandLim->at.limit_.at(cw) = assignedBytes.at(i);
            }

            // signal a retransmission
            // schedule list contains number of granted blocks
            scheduler_->scheduleList_[carrierFrequency][scListId] = cwAllocatedBlocks;
            // mark codeword as used
            if (scheduler_->allocatedCws_.find(senderId) != scheduler_->allocatedCws_.end()) {
                scheduler_->allocatedCws_.at(senderId)++;
            }
            else {
                scheduler_->allocatedCws_[senderId] = 1;
            }

            EV << NOW << " D2dRtxScheduling::schedulePerAcidRtxD2D HARQ Process " << (unsigned int)acid << " : " << bytes << " bytes served! " << endl;

            currentProcess->markSelected(cw);

            return bytes;
        }
    }
    catch (std::exception& e) {
        throw cRuntimeError("Exception in D2dRtxScheduling::schedulePerAcidRtxD2D(): %s", e.what());
    }
    return 0;
}

} //namespace
