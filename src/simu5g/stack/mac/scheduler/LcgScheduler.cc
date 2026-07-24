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

#include "simu5g/stack/mac/scheduler/LcgScheduler.h"
#include "simu5g/stack/mac/buffer/LteMacBuffer.h"

namespace simu5g {

using namespace omnetpp;

LcgScheduler::LcgScheduler(LteMacUe *mac) : lastExecutionTime_(0), mac_(mac)
{
}

LcgScheduler& LcgScheduler::operator=(const LcgScheduler& other)
{
    if (&other == this)
        return *this;

    lastExecutionTime_ = other.lastExecutionTime_;
    mac_ = other.mac_;
    ueScheduler_ = other.ueScheduler_;
    scheduleList_ = other.scheduleList_;
    scheduledBytesList_ = other.scheduledBytesList_;
    scheduledSoPduSizes_ = other.scheduledSoPduSizes_;
    statusMap_ = other.statusMap_;

    return *this;
}


ScheduleList& LcgScheduler::schedule(unsigned int availableBytes, Direction grantDir)
{
    // Clean up old schedule decisions.
    // For each cid, this map will store the amount of sent data (in SDUs).
    scheduleList_.clear();

    // Clean up old schedule decisions.
    // For each cid, this map will store the amount of sent data (in bytes, useful for macSduRequest).
    scheduledBytesList_.clear();

    // Clean up the NR-SO per-PDU plan (one entry per planned PDU for SO connections).
    scheduledSoPduSizes_.clear();

    // Clean up scheduling support status map
    statusMap_.clear();

    // If true, assure a minimum reserved rate to all connections (LCP first
    // phase), if false, provide a best effort service (LCP second phase)
    bool priorityService = true;

    bool firstSdu = true;

    const LcgMap& lcgMap = mac_->getLcgMap();

    if (lcgMap.empty())
        return scheduleList_;

    // for all traffic classes
    for (unsigned short i = 0; i < UNKNOWN_TRAFFIC_TYPE; ++i) {
        // Prepare the iterators to cycle the entire scheduling set
        auto it_pair = lcgMap.equal_range((LteTrafficClass)i);
        auto it = it_pair.first, et = it_pair.second;

        EV << NOW << " LcgScheduler::schedule - Node  " << mac_->getMacNodeId() << ", Starting priority service for traffic class " << i << endl;

        // -------------------------------------------------------------------------------------------------- //
        // FIXME This is a workaround in case a UE has both UL and D2D active connections.
        //       When an UL grant has been received, check if there is data in the D2D connections' buffer
        //       and if the bsrTriggered flag is set. If so, do not schedule any UL connection and use the
        //       grant for sending the BSR related to the D2D connection(s).
        //       A smarter policy should be implemented
        if (grantDir == UL && mac_->bsrTriggered()) {
            // look for an active D2D connection
            for (it = it_pair.first; it != et; ++it) {
                // get the Flow descriptor
                MacCid cid = it->second.first;
                const FlowDescriptor& connDesc = mac_->getConnDesc(cid);
                if (connDesc.getDirection() == D2D) {
                    // get the connection virtual buffer
                    LteMacBuffer *vQueue = it->second.second;
                    // get the buffer size
                    unsigned int queueLength = vQueue->getQueueOccupancy(); // in bytes
                    if (queueLength != 0)
                        return scheduleList_;
                }
            }
        }
        // -------------------------------------------------------------------------------------------------- //

        //! FIXME Allocation of the same resource to flows with the same priority not implemented
        for (it = it_pair.first; it != et; ++it) {
            // processing all connections of the same traffic class

            // get the connection virtual buffer
            LteMacBuffer *vQueue = it->second.second;

            // get the buffer size
            unsigned int queueLength = vQueue->getQueueOccupancy(); // in bytes

            // connection id of the processed connection
            MacCid cid = it->second.first;

            // get the Flow descriptor
            const FlowDescriptor& connDesc = mac_->getConnDesc(cid);
            // TODO get the QoS parameters

            // connection must have the same direction as the grant
            if (connDesc.getDirection() != grantDir)
                continue;

            unsigned int toServe = queueLength;
            // Check whether the virtual buffer is empty
            if (queueLength == 0) {
                EV << "LcgScheduler::schedule scheduled connection is no longer active " << endl;
                continue; // go to next connection
            }
            else {
                // we need to consider also the size of RLC and MAC headers
                if (connDesc.getRlcType() == UM)
                    toServe += RLC_HEADER_UM;
                else if (connDesc.getRlcType() == AM)
                    toServe += RLC_HEADER_AM;

                if (firstSdu)
                    toServe += MAC_HEADER;
            }

            // get a pointer to the appropriate status element: we need a tracing element
            // in order to store information about connections and data transmitted. These
            // information may be consulted at the end of the LCP algorithm
            StatusElem *elem;

            if (statusMap_.find(cid) == statusMap_.end()) {
                // the element does not exist, initialize it
                elem = &statusMap_[cid];
                elem->occupancy_ = vQueue->getQueueLength();
                elem->sentData_ = 0;
                elem->sentSdus_ = 0;
                // TODO set bucket from QoS parameters
                elem->bucket_ = 1000;
            }
            else {
                elem = &statusMap_[cid];
            }

            EV << NOW << " LcgScheduler::schedule Node " << mac_->getMacNodeId() << " , Parameters:" << endl;
            EV << "\t Logical Channel ID: " << cid.getLcid() << endl;
            EV << "\t CID: " << cid << endl;
            if (priorityService) {
                // Update bucket value for this connection

                // get the actual bucket value and the configured max size
                double bucket = elem->bucket_; // TODO parameters -> bucket ;
                double maximumBucketSize = 10000.0; // TODO  parameters -> maxBurst;

                EV << NOW << " LcgScheduler::schedule Bucket size: " << bucket << " bytes (max size " << maximumBucketSize << " bytes) - BEFORE SERVICE " << endl;

                // if the connection started before the last scheduling event, use the
                // global time interval
                if (lastExecutionTime_ > 0) { // TODO desc->parameters_.startTime_) {
                    // PBR*(n*TTI) where n is the number of TTI from last update
                    bucket += /* TODO desc->parameters_.minReservedRate_*/ 100.0 * TTI;
                }
                // otherwise, set the bucket value accordingly to the start time
                else {
                    simtime_t localTimeInterval = NOW - 0 /* TODO desc->parameters_.startTime_ */;
                    if (localTimeInterval < 0)
                        localTimeInterval = 0;

                    bucket = /* TODO desc->parameters_.minReservedRate_*/ 100.0 * localTimeInterval.dbl();
                }

                // do not overflow the maximum bucket size
                if (bucket > maximumBucketSize)
                    bucket = maximumBucketSize;

                // update connection's bucket
// TODO                desc->parameters_.bucket_ = bucket;

                // update the tracing element accordingly
                elem->bucket_ = 100.0; // TODO desc->parameters_.bucket_;
                EV << NOW << " LcgScheduler::schedule Bucket size: " << bucket << " bytes (max size " << maximumBucketSize << " bytes) - AFTER SERVICE " << endl;
            }

            EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << ", remaining grant: " << availableBytes << " bytes " << endl;
            EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << " buffer Size: " << toServe << " bytes " << endl;

            int minBytes = firstSdu ? MAC_HEADER + RLC_HEADER_UM : RLC_HEADER_UM;

            // If priority service: (availableBytes>0) && (desc->buffer_.occupancy() > 0) && (desc->parameters_.bucket_ > 0)
            // If best effort service: (availableBytes>0) && (desc->buffer_.occupancy() > 0)
            if ((availableBytes > minBytes) && (toServe > 0)
                && (!priorityService || true /*TODO (desc->parameters_.bucket_ > 0)*/))
            {
                // Check if it is possible to serve the sdu, depending on the constraint
                // of the type of service
                // Priority service:
                //    ( sdu->size() <= availableBytes) && ( sdu->size() <= desc->parameters_.bucket_)
                // Best Effort service:
                //    ( sdu->size() <= availableBytes) && (!priorityService_)
                if (connDesc.getSoFraming()) {
                    // NR-SO: the RLC emits one SDU/segment per PDU (no concatenation),
                    // so fill the grant by multiplexing several PDUs into it. Record
                    // each PDU's payload size; the MAC issues one SDU request per entry.
                    // Continuation state is shared across this UE's per-carrier schedulers
                    // (they all drain the same RLC TX buffer), so a carrier reserves the
                    // header matching segmentation another carrier already did this TTI.
                    std::map<MacCid, bool>& soFrontIsContinuation = mac_->getSoContinuationMap();
                    std::vector<unsigned int>& pduSizes = scheduledSoPduSizes_[cid];
                    int budget = (int)availableBytes;
                    while (!vQueue->isEmpty()) {
                        int macHdr = (firstSdu ? MAC_HEADER : 0);
                        // RLC header for this PDU: the exact segment-state header the RLC TX
                        // will emit, so the carve and drain match byte-for-byte (sized for the
                        // flow's SN field length). Only the front SDU can be a continuation.
                        // UM: complete=1B (no SN) if the whole remaining SDU fits, else
                        // first/continuation per nrUmHeaderBytes. AM always carries the SN
                        // (nrAmHeaderBytes). TS 38.322 6.2.1.3/6.2.1.4.
                        unsigned int snBits = connDesc.getRlcSnFieldLength();
                        unsigned int rlcHdr;
                        if (connDesc.getRlcType() == AM) {
                            rlcHdr = nrAmHeaderBytes(soFrontIsContinuation[cid] ? NRUM_CONTINUATION : NRUM_FIRST, snBits);
                        }
                        else {
                            int remaining = vQueue->front().first;
                            NrUmSegState st = soFrontIsContinuation[cid] ? NRUM_CONTINUATION
                                            : (remaining + 1 <= budget - macHdr) ? NRUM_COMPLETE
                                            : NRUM_FIRST;
                            rlcHdr = nrUmHeaderBytes(st, snBits);
                        }
                        int hdr = (int)rlcHdr + macHdr;
                        if (budget <= hdr)
                            break; // no room for another PDU (header + at least 1 payload byte)
                        int room = budget - hdr;
                        PacketInfo info = vQueue->popFront();
                        unsigned int payload;
                        bool segmented = false;
                        if ((int)info.first <= room) {
                            payload = info.first; // whole (remaining) SDU fits in this PDU
                            soFrontIsContinuation[cid] = false; // SDU fully sent
                        }
                        else {
                            payload = (unsigned int)room; // segment; remainder stays queued
                            info.first -= room;
                            vQueue->pushFront(info);
                            segmented = true;
                            soFrontIsContinuation[cid] = true; // remainder is a continuation
                        }
                        // The MAC request size includes the RLC header (rlcPduMakeNr
                        // subtracts it before segmenting), so add it back here so the
                        // RLC carves exactly this payload.
                        pduSizes.push_back(payload + rlcHdr);
                        int pduBytes = hdr + (int)payload;
                        budget -= pduBytes;
                        elem->sentData_ += pduBytes;
                        elem->sentSdus_++;
                        firstSdu = false;
                        if (segmented)
                            break; // grant exhausted by the segment
                    }
                    elem->occupancy_ = vQueue->getQueueOccupancy();
                    availableBytes = budget > 0 ? (unsigned int)budget : 0;
                    toServe = 0;
                    if (vQueue->isEmpty())
                        soFrontIsContinuation[cid] = false; // buffer drained; next SDU is fresh
                    if (pduSizes.empty())
                        scheduledSoPduSizes_.erase(cid);
                }
                else if ((toServe <= availableBytes) /*&& ( !priorityService || ( sduSize <= 0) ) // TODO desc->parameters_.bucket_*/) {
                    // remove SDU from virtual buffer
                    vQueue->popFront();

                    if (priorityService) {
                    }

                    // check if there is space for a SDU
                    int alloc = toServe;
                    if (firstSdu) {
                        alloc -= MAC_HEADER;
                        firstSdu = false;
                    }

                    // update the tracing element
                    elem->occupancy_ = vQueue->getQueueOccupancy();
                    elem->sentData_ += alloc;

                    if (connDesc.getRlcType() == UM)
                        alloc -= RLC_HEADER_UM;
                    else if (connDesc.getRlcType() == AM)
                        alloc -= RLC_HEADER_AM;

                    if (alloc > 0)
                        elem->sentSdus_++;

                    availableBytes -= toServe;

                    while (!vQueue->isEmpty()) {
                        // remove SDUs from virtual buffer
                        vQueue->popFront();
                    }

                    toServe = 0;

                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << ",  SDU of size " << elem->sentData_ << " selected for transmission" << endl;
                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << ", remaining grant: " << availableBytes << " bytes" << endl;
                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << " buffer Size: " << toServe << " bytes" << endl;
                }
                else {

                    if (priorityService) {
                    }

                    int alloc = availableBytes;
                    if (firstSdu) {
                        alloc -= MAC_HEADER;
                        firstSdu = false;
                    }

                    // update the tracing element
                    elem->occupancy_ = vQueue->getQueueOccupancy();
                    elem->sentData_ += alloc;

                    if (connDesc.getRlcType() == UM)
                        alloc -= RLC_HEADER_UM;
                    else if (connDesc.getRlcType() == AM)
                        alloc -= RLC_HEADER_AM;

                    // check if there is space for a SDU
                    if (alloc > 0)
                        elem->sentSdus_++;

                    // update buffer
                    while (alloc > 0) {
                        // update pkt info
                        PacketInfo newPktInfo = vQueue->popFront();
                        if (newPktInfo.first > alloc) {
                            newPktInfo.first = newPktInfo.first - alloc;
                            vQueue->pushFront(newPktInfo);
                            alloc = 0;
                        }
                        else {
                            alloc -= newPktInfo.first;
                        }
                    }

                    toServe -= availableBytes;
                    availableBytes = 0;

                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << ",  SDU of size " << elem->sentData_ << " selected for transmission" << endl;
                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << ", remaining grant: " << availableBytes << " bytes" << endl;
                    EV << NOW << " LcgScheduler::schedule - Node " << mac_->getMacNodeId() << " buffer Size: " << toServe << " bytes" << endl;
                }
            }

            // check if flow is still backlogged
            if (availableBytes > minBytes) {
                // TODO the priority is higher when the associated integer is lower ( e.g. priority 2 is
                // greater than 4 )
            }
            if (elem->sentSdus_ > 0) {
                // update the last schedule time
                lastExecutionTime_ = NOW;

                // signal service for current connection
                unsigned int *servicedSdu = nullptr;

                if (scheduleList_.find(cid) == scheduleList_.end()) {
                    // the element does not exist, initialize it
                    servicedSdu = &scheduleList_[cid];
                    *servicedSdu = elem->sentSdus_;
                }
                else {
                    // connection already scheduled during this TTI
                    servicedSdu = &scheduleList_.at(cid);
                }

                // update scheduled bytes
                if (scheduledBytesList_.find(cid) == scheduledBytesList_.end()) {
                    scheduledBytesList_[cid] = elem->sentData_;
                }
                else {
                    scheduledBytesList_[cid] += elem->sentData_;
                }
            }
            // If the end of the connections map is reached and we were on priority and on last traffic class
            if (priorityService && (it == et) && ((i + 1) == (unsigned short)UNKNOWN_TRAFFIC_TYPE)) {
                // the first phase of the LCP algorithm has completed ...
                // ... switch to best effort allocation!
                priorityService = false;
                //  reset traffic class
                i = 0;
                EV << "LcgScheduler::schedule - Node" << mac_->getMacNodeId() << ", Starting best effort service" << endl;
            }
        } // END of connections cycle
    } // END of Traffic Classes cycle

    return scheduleList_;
}

ScheduleList& LcgScheduler::getScheduledBytesList()
{
    return scheduledBytesList_;
}

const std::vector<unsigned int> *LcgScheduler::getScheduledSoPduSizes(MacCid cid) const
{
    auto it = scheduledSoPduSizes_.find(cid);
    return it != scheduledSoPduSizes_.end() ? &it->second : nullptr;
}

} //namespace simu5g
