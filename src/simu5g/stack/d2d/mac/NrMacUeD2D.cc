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

// NOTE: this class mirrors ~LteMacUeD2D on top of the NR base ~NrMacUe.
// The D2D deltas cloned from LteMacUeD2D (initialize, handleMessage, checkRAC,
// macHandleGrant, macHandleRac, createRx/TxHarqBuffer, doHandover,
// macHandleD2DModeSwitch) are kept in sync with the sibling implementation in
// LteMacUeD2D.cc; handleSelfMessage()/macPduMake() are the numerology-aware
// NR versions that previously lived directly in NrMacUe.

#include "simu5g/stack/d2d/mac/NrMacUeD2D.h"

#include <inet/common/TimeTag_m.h>

#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/mac/amc/LteAmc.h"
#include "simu5g/stack/mac/buffer/LteMacBuffer.h"
#include "simu5g/stack/mac/buffer/LteMacQueue.h"
#include "simu5g/stack/mac/buffer/harq/LteHarqBufferRx.h"
#include "simu5g/stack/mac/buffer/harq_d2d/LteHarqBufferRxD2D.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/mac/packet/LteRac_m.h"
#include "simu5g/stack/mac/packet/LteSchedulingGrant.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerUeUl.h"
#include "simu5g/stack/d2d/mac/scheduler/LcgSchedulerD2D.h"

namespace simu5g {

Define_Module(NrMacUeD2D);

using namespace inet;

// NOTE: the signal name is interned at runtime here -- see the comment on
// d2dUeHelper_ in the header for why there is deliberately NO static
// registerSignal() in this translation unit.
NrMacUeD2D::NrMacUeD2D() : d2dUeHelper_(this, cComponent::registerSignal("rcvdD2DModeSwitchNotification"))
{
}

void NrMacUeD2D::initialize(int stage)
{
    NrMacUe::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_AMC_ATTACHUSER) {
        // get parameters
        d2dUeHelper_.setUsePreconfiguredTxParams(par("usePreconfiguredTxParams"));

        if (cellId_ != NODEID_NONE) {
            d2dUeHelper_.rebuildPreconfiguredTxParams(binder_);

            // get the reference to the eNB
            d2dUeHelper_.setEnb(check_and_cast<ID2dMacEnb *>(binder_->getMacByNodeId(cellId_)));

            LteAmc *amc = check_and_cast<LteMacEnb *>(binder_->getMacByNodeId(cellId_))->getAmc();
            amc->attachUser(nodeId_, D2D);

// TODO remove it. UeCollector connection made in LteMacUe Initialize
        }
    }
}

void NrMacUeD2D::handleSelfMessage()
{
    EV << "----- UE MAIN LOOP -----" << endl;

    // extract PDUs from all HARQ RX buffers and pass them to unmaker
    for (auto& [carrierFreq, harqRxMap] : harqRxBuffers_) {
        if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq(carrierFreq)) > 0)
            continue;

        std::list<Packet *> pduList;
        for (auto [macNodeId, rxBuf] : harqRxMap) {
            pduList = rxBuf->extractCorrectPdus();
            while (!pduList.empty()) {
                auto pdu = pduList.front();
                pduList.pop_front();
                macPduUnmake(pdu);
            }
        }
    }

    EV << NOW << "NrMacUe::handleSelfMessage " << nodeId_ << " - HARQ process " << (unsigned int)currentHarq_ << endl;

    // no grant available - if user has backlogged data, it will trigger scheduling request
    // no HARQ counter is updated since no transmission is sent.

    bool noSchedulingGrants = true;
    for (auto& [carrierFreq, grant] : schedulingGrant_) {
        if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq(carrierFreq)) > 0)
            continue;

        if (grant != nullptr)
            noSchedulingGrants = false;
    }

    if (noSchedulingGrants) {
        EV << NOW << " NrMacUe::handleSelfMessage " << nodeId_ << " NO configured grant" << endl;
        checkRAC();
        // TODO ensure all operations done before return (i.e. move H-ARQ RX purge before this point)
    }
    else {
        bool periodicGrant = false;
        bool checkRac = false;
        bool skip = false;
        for (auto& [carrierFreq, grant] : schedulingGrant_) {
            if (grant != nullptr && grant->getPeriodic()) {
                periodicGrant = true;

                // Periodic checks
                if (--expirationCounter_[carrierFreq] < 0) {
                    // Periodic grant is expired
                    grant = nullptr;
                    checkRac = true;
                }
                else if (--periodCounter_[carrierFreq] > 0) {
                    skip = true;
                }
                else {
                    // resetting grant period
                    periodCounter_[carrierFreq] = grant->getPeriod();
                    // this is periodic grant TTI - continue with frame sending
                    checkRac = false;
                    skip = false;
                    break;
                }
            }
        }
        if (periodicGrant) {
            if (checkRac)
                checkRAC();
            else {
                if (skip)
                    return;
            }
        }
    }

    scheduleList_.clear();
    requestedSdus_ = 0;
    if (!noSchedulingGrants) { // if a grant is configured
        EV << NOW << " NrMacUe::handleSelfMessage " << nodeId_ << " entered scheduling" << endl;

        bool retx = false;

        if (!firstTx) {
            EV << "\t currentHarq_ counter initialized " << endl;
            firstTx = true;
            // the gNb will receive the first PDU in 2 TTI, thus initializing acid to 0
            currentHarq_ = harqProcesses_ - 2;
        }

        for (auto& [carrierFrequency, harqTxBufferMap] : harqTxBuffers_) {
            // skip if this is not the turn of this carrier
            if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFrequency))) > 0)
                continue;

            // skip if no grant is configured for this carrier
            if (schedulingGrant_.find(carrierFrequency) == schedulingGrant_.end() || schedulingGrant_[carrierFrequency] == nullptr)
                continue;

            for (auto [it2Key, currHarq] : harqTxBufferMap) {
                unsigned int numProcesses = currHarq->getNumProcesses();

                for (unsigned int proc = 0; proc < numProcesses; proc++) {
                    LteHarqProcessTx *currProc = currHarq->getProcess(proc);

                    // check if the current process has unit ready for retransmission
                    bool ready = currProc->hasReadyUnits();
                    CwList cwListRetx = currProc->readyUnitsIds();

                    EV << "\t [process=" << proc << "] , [retx=" << (ready ? "true" : "false") << "] , [n=" << cwListRetx.size() << "]" << endl;

                    // check if one 'ready' unit has the same direction of the grant
                    bool checkDir = false;
                    for (Codeword cw : cwListRetx) {
                        auto info = currProc->getPdu(cw)->getTag<UserControlInfo>();
                        if (info->getDirection() == schedulingGrant_[carrierFrequency]->getDirection()) {
                            checkDir = true;
                            break;
                        }
                    }

                    // if a retransmission is needed
                    if (ready && checkDir) {
                        UnitList signal;
                        signal.first = proc;
                        signal.second = cwListRetx;
                        currHarq->markSelected(signal, schedulingGrant_[carrierFrequency]->getUserTxParams()->getLayers().size());
                        retx = true;
                        break;
                    }
                }
            }
        }
        // if no retransmission is needed, proceed with normal scheduling
        if (!retx) {
            emptyScheduleList_ = true;
            std::map<GHz, LteSchedulerUeUl *>::iterator sit;
            for (auto [carrierFrequency, carrierLcgScheduler] : lcgScheduler_) {
                // skip if this is not the turn of this carrier
                if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFrequency))) > 0)
                    continue;

                EV << "NrMacUe::handleSelfMessage - running LCG scheduler for carrier [" << carrierFrequency << "]" << endl;
                LteMacScheduleList *carrierScheduleList = carrierLcgScheduler->schedule();
                EV << "NrMacUe::handleSelfMessage - scheduled " << carrierScheduleList->size() << " connections on carrier " << carrierFrequency << endl;
                scheduleList_[carrierFrequency] = carrierScheduleList;
                if (!carrierScheduleList->empty())
                    emptyScheduleList_ = false;
            }

            if ((bsrTriggered_ || d2dUeHelper_.getBsrD2DMulticastTriggered()) && emptyScheduleList_) {
                // no connection scheduled, but we can use this grant to send a BSR to the eNB
                macPduMake();
            }
            else {
                requestedSdus_ = macSduRequest(); // returns an integer
            }
        }

        // Message that triggers flushing of Tx H-ARQ buffers for all users
        // This way, flushing is performed after the (possible) reception of new MAC PDUs
        cMessage *flushHarqMsg = new cMessage("flushHarqMsg");
        flushHarqMsg->setSchedulingPriority(1);        // after other messages
        scheduleAt(NOW, flushHarqMsg);
    }

    //============================ DEBUG ==========================
    if (debugHarq_) {
        for (auto& [carrierFreq, harqTxBufferMap] : harqTxBuffers_) {
            EV << "\n carrier[ " << carrierFreq << "] htxbuf.size " << harqTxBufferMap.size() << endl;
            EV << "\n htxbuf.size " << harqTxBuffers_.size() << endl;

            int cntOuter = 0;
            int cntInner = 0;
            for (auto [currId, currHarq] : harqTxBufferMap) {
                BufferStatus harqStatus = currHarq->getBufferStatus();
                EV << "\t cycleOuter " << cntOuter << " - bufferStatus.size=" << harqStatus.size() << endl;
                for (const auto& jt : harqStatus) {
                    EV << "\t\t cycleInner " << cntInner << " - jt->size=" << jt.size()
                       << " - statusCw(0/1)=" << jt.at(0).second << "/" << jt.at(1).second << endl;
                }
            }
        }
    }
    //======================== END DEBUG ==========================

    // update current HARQ process id, if needed
    if (requestedSdus_ == 0) {
        EV << NOW << " NrMacUe::handleSelfMessage - incrementing counter for HARQ processes " << (unsigned int)currentHarq_ << " --> " << (currentHarq_ + 1) % harqProcesses_ << endl;
        currentHarq_ = (currentHarq_ + 1) % harqProcesses_;
    }

    decreaseNumerologyPeriodCounter();

    EV << "--- END UE MAIN LOOP ---" << endl;
}

void NrMacUeD2D::macPduMake(MacCid cid)
{
    int64_t size = 0;

    macPduList_.clear();

    bool bsrAlreadyMade = false;
    // UE is in D2D-mode but it received an UL grant (for BSR)
    for (auto& [carrierFreq, grant] : schedulingGrant_) {
        // skip if this is not the turn of this carrier
        if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFreq))) > 0)
            continue;

        if (grant != nullptr && grant->getDirection() == UL && emptyScheduleList_) {
            if (bsrTriggered_ || d2dUeHelper_.getBsrD2DMulticastTriggered()) {
                // Compute BSR size taking into account only DM flows
                int sizeBsr = 0;
                for (auto [cid, connInfo] : connDescOut_) {
                    Direction connDir = connInfo.flowInfo.getDirection();
                    LteMacBuffer* buffer = connInfo.buffer;

                    // if the bsr was triggered by D2D (D2D_MULTI), only account for D2D (D2D_MULTI) connections
                    if (bsrTriggered_ && connDir != D2D)
                        continue;
                    if (d2dUeHelper_.getBsrD2DMulticastTriggered() && connDir != D2D_MULTI)
                        continue;

                    sizeBsr += buffer->getQueueOccupancy();

                    // take into account the RLC header size
                    if (sizeBsr > 0) {
                        if (connInfo.flowInfo.getRlcType() == UM)
                            sizeBsr += RLC_HEADER_UM;
                        else if (connInfo.flowInfo.getRlcType() == AM)
                            sizeBsr += RLC_HEADER_AM;
                    }
                }

                if (sizeBsr > 0) {
                    // Call the appropriate function for making a BSR for D2D communication
                    LogicalCid bsrType = d2dUeHelper_.getBsrD2DMulticastTriggered() ? D2D_MULTI_SHORT_BSR : D2D_SHORT_BSR;
                    d2dUeHelper_.setBsrD2DMulticastTriggered(false);
                    Packet *macPktBsr = d2dUeHelper_.makeBsr(sizeBsr);
                    auto info = macPktBsr->getTagForUpdate<UserControlInfo>();
                    info->setPacketLcid(bsrType);
                    info->setCarrierFrequency(carrierFreq);
                    info->setUserTxParams(grant->getUserTxParams()->dup());

                    // Add the created BSR to the PDU List
                    LteChannelModel *channelModel = phy_->getChannelModel();
                    if (channelModel == nullptr)
                        throw cRuntimeError("NrMacUe::macPduMake - channel model is a null pointer");
                    macPduList_[channelModel->getCarrierFrequency()][{getMacCellId(), 0}] = macPktBsr;
                    bsrAlreadyMade = true;
                    EV << "NrMacUe::macPduMake - BSR D2D created with size " << sizeBsr << " created" << endl;

                    bsrRtxTimer_ = bsrRtxTimerStart_;  // this prevents the UE from sending an unnecessary RAC request
                }
                else {
                    d2dUeHelper_.setBsrD2DMulticastTriggered(false);
                    bsrTriggered_ = false;
                    bsrRtxTimer_ = 0;
                }
            }
            break;
        }
    }

    if (!bsrAlreadyMade) {
        // In a D2D communication if BSR was created above this part isn't executed
        // Build a MAC PDU for each scheduled user on each codeword
        for (auto [carrierFreq, schList] : scheduleList_) {
            // skip if this is not the turn of this carrier
            if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFreq))) > 0)
                continue;

            LteMacScheduleList::const_iterator it;
            for (auto& item : *schList) {
                Packet *macPkt;

                MacCid destCid = item.first.first;
                Codeword cw = item.first.second;

                // get the direction (UL/D2D/D2D_MULTI) and the corresponding destination ID
                const FlowDescriptor& connInfo = connDescOut_.at(destCid).flowInfo;
                MacNodeId destId = connInfo.getDestId();
                Direction dir = connInfo.getDirection();

                std::pair<MacNodeId, Codeword> pktId = {destId, cw};
                unsigned int sduPerCid = item.second;

                if (sduPerCid == 0 && !bsrTriggered_ && !d2dUeHelper_.getBsrD2DMulticastTriggered())
                    continue;

                if (macPduList_.find(carrierFreq) == macPduList_.end()) {
                    MacPduList newList;
                    macPduList_[carrierFreq] = newList;
                }
                MacPduList::iterator pit = macPduList_[carrierFreq].find(pktId);

                // No packets for this user on this codeword
                if (pit == macPduList_[carrierFreq].end()) {
                    // Create a PDU
                    macPkt = new Packet("LteMacPdu");
                    auto header = makeShared<LteMacPdu>();
                    header->setHeaderLength(MAC_HEADER);
                    macPkt->insertAtFront(header);

                    macPkt->addTagIfAbsent<CreationTimeTag>()->setCreationTime(NOW);
                    macPkt->addTagIfAbsent<UserControlInfo>()->setSourceId(getMacNodeId());
                    macPkt->addTagIfAbsent<UserControlInfo>()->setDestId(destId);
                    macPkt->addTagIfAbsent<UserControlInfo>()->setDirection(dir);
                    macPkt->addTagIfAbsent<UserControlInfo>()->setPacketLcid(SHORT_BSR);
                    macPkt->addTagIfAbsent<UserControlInfo>()->setCarrierFrequency(carrierFreq);

                    macPkt->addTagIfAbsent<UserControlInfo>()->setGrantId(schedulingGrant_[carrierFreq]->getGrantId());

                    if (d2dUeHelper_.getUsePreconfiguredTxParams())
                        macPkt->addTagIfAbsent<UserControlInfo>()->setUserTxParams(d2dUeHelper_.getPreconfiguredTxParams()->dup());
                    else
                        macPkt->addTagIfAbsent<UserControlInfo>()->setUserTxParams(schedulingGrant_[carrierFreq]->getUserTxParams()->dup());

                    macPduList_[carrierFreq][pktId] = macPkt;
                }
                else {
                    // Never goes here because of the macPduList_.clear() at the beginning
                    macPkt = pit->second;
                }

                while (sduPerCid > 0) {
                    // Add SDU to PDU
                    // Find Mac Pkt
                    if (connDescOut_.find(destCid) == connDescOut_.end())
                        throw cRuntimeError("Unable to find mac buffer for cid %s", destCid.str().c_str());

                    if (connDescOut_[destCid].queue->isEmpty())
                        throw cRuntimeError("Empty buffer for cid %s, while expected SDUs were %d", destCid.str().c_str(), sduPerCid);

                    auto pkt = check_and_cast<Packet *>(connDescOut_[destCid].queue->popFront());

                    // multicast support
                    // this trick gets the group ID from the MAC SDU and sets it in the MAC PDU
                    auto flowInfo = pkt->getTag<FlowControlInfo>();
                    MacNodeId groupId = flowInfo->getMulticastGroupId();
                    if (groupId != NODEID_NONE) // for unicast, group id is -1
                        macPkt->getTagForUpdate<UserControlInfo>()->setPacketMulticastGroupId(groupId);

                    drop(pkt);

                    auto macPdu = macPkt->removeAtFront<LteMacPdu>();

                    macPdu->pushSdu(pkt, destCid.getLcid());
                    macPkt->insertAtFront(macPdu);
                    sduPerCid--;
                }

                // consider virtual buffers to compute BSR size
                size += connDescOut_[destCid].buffer->getQueueOccupancy();

                if (size > 0) {
                    // take into account the RLC header size
                    if (connDescOut_[destCid].flowInfo.getRlcType() == UM)
                        size += RLC_HEADER_UM;
                    else if (connDescOut_[destCid].flowInfo.getRlcType() == AM)
                        size += RLC_HEADER_AM;
                }
            }
        }
    }

    // Put MAC PDUs in H-ARQ buffers
    for (auto& [carrierFreq, macPduMap] : macPduList_) {
        // skip if this is not the turn of this carrier
        if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFreq))) > 0)
            continue;

        if (harqTxBuffers_.find(carrierFreq) == harqTxBuffers_.end()) {
            HarqTxBuffers newHarqTxBuffers;
            harqTxBuffers_[carrierFreq] = newHarqTxBuffers;
        }
        HarqTxBuffers& harqTxBuffers = harqTxBuffers_[carrierFreq];

        for (auto& [pktId, macPkt] : macPduMap) {
            MacNodeId destId = pktId.first;
            Codeword cw = pktId.second;
            // Check if the HarqTx buffer already exists for the destId
            // Get a reference for the destId TXBuffer
            LteHarqBufferTx *txBuf;
            HarqTxBuffers::iterator hit = harqTxBuffers.find(destId);
            if (hit != harqTxBuffers.end()) {
                // The tx buffer already exists
                txBuf = hit->second;
            }
            else {
                // The tx buffer does not exist yet for this mac node id, create one
                // FIXME: hb is never deleted
                LteHarqBufferTx *hb = createTxHarqBuffer(destId, (Direction)macPkt->getTag<UserControlInfo>()->getDirection());
                harqTxBuffers[destId] = hb;
                txBuf = hb;
            }

            // search for an empty unit within the first available process
            UnitList txList = (macPkt->getTag<UserControlInfo>()->getDirection() == D2D_MULTI) ? txBuf->getEmptyUnits(currentHarq_) : txBuf->firstAvailable();
            EV << "NrMacUe::macPduMake - [Used Acid=" << (unsigned int)txList.first << "]" << endl;

            // BSR related operations

               // according to the TS 36.321 v8.7.0, when there are uplink resources assigned to the UE, a BSR
               // has to be sent even if there is no data in the user's queues. In few words, a BSR is always
               // triggered and has to be sent when there are enough resources

               // TODO implement differentiated BSR attach
               //
               //            // if there's enough space for a LONG BSR, send it
               //            if( (availableBytes >= LONG_BSR_SIZE) ) {
               //                // Create a PDU if data were not scheduled
               //                if (pdu==0)
               //                    pdu = new LteMacPdu();
               //
               //                if(LteDebug::trace("LteSchedulerUeUl::schedule") || LteDebug::trace("LteSchedulerUeUl::schedule@bsrTracing"))
               //                    fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - node %hu, sending a Long BSR...\n",NOW,nodeId);
               //
               //                // create a full BSR
               //                pdu->ctrlPush(fullBufferStatusReport());
               //
               //                // do not reset BSR flag
               //                mac_->bsrTriggered() = true;
               //
               //                availableBytes -= LONG_BSR_SIZE;
               //
               //            }
               //
               //            // if there's space only for a SHORT BSR and there are scheduled flows, send it
               //            else if( (mac_->bsrTriggered() == true) && (availableBytes >= SHORT_BSR_SIZE) && (highestBackloggedFlow != -1) ) {
               //
               //                // Create a PDU if data were not scheduled
               //                if (pdu==0)
               //                    pdu = new LteMacPdu();
               //
               //                if(LteDebug::trace("LteSchedulerUeUl::schedule") || LteDebug::trace("LteSchedulerUeUl::schedule@bsrTracing"))
               //                    fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - node %hu, sending a Short/Truncated BSR...\n",NOW,nodeId);
               //
               //                // create a short BSR
               //                pdu->ctrlPush(shortBufferStatusReport(highestBackloggedFlow));
               //
               //                // do not reset BSR flag
               //                mac_->bsrTriggered() = true;
               //
               //                availableBytes -= SHORT_BSR_SIZE;
               //
               //            }
               //            // if there's a BSR triggered but there's not enough space, collect the appropriate statistic
               //            else if(availableBytes < SHORT_BSR_SIZE && availableBytes < LONG_BSR_SIZE) {
               //                Stat::put(LTE_BSR_SUPPRESSED_NODE,nodeId,1.0);
               //                Stat::put(LTE_BSR_SUPPRESSED_CELL,mac_->cellId(),1.0);
               //            }
               //            Stat::put (LTE_GRANT_WASTED_BYTES_UL, nodeId, availableBytes);
               //        }
               //
               //        // 4) PDU creation
               //
               //        if (pdu!=0) {
               //
               //            pdu->cellId() = mac_->cellId();
               //            pdu->nodeId() = nodeId;
               //            pdu->direction() = mac::UL;
               //            pdu->error() = false;
               //
               //            if(LteDebug::trace("LteSchedulerUeUl::schedule"))
               //                fprintf(stderr, "%.9f LteSchedulerUeUl::schedule - node %hu, creating uplink PDU.\n", NOW, nodeId);
               //
               //        } */

            auto macPdu = macPkt->removeAtFront<LteMacPdu>();
            // Attach BSR to PDU if RAC is won and wasn't already made
            if ((bsrTriggered_ || d2dUeHelper_.getBsrD2DMulticastTriggered()) && !bsrAlreadyMade && size > 0) {
                MacBsr *bsr = new MacBsr();
                bsr->setTimestamp(simTime().dbl());
                bsr->setSize(size);
                macPdu->pushCe(bsr);
                bsrTriggered_ = false;
                d2dUeHelper_.setBsrD2DMulticastTriggered(false);
                bsrAlreadyMade = true;
                EV << "NrMacUe::macPduMake - BSR created with size " << size << endl;
            }

            if (bsrAlreadyMade && size > 0) { // this prevents the UE from sending an unnecessary RAC request
                bsrRtxTimer_ = bsrRtxTimerStart_;
            }
            else
                bsrRtxTimer_ = 0;

            macPkt->insertAtFront(macPdu);

            EV << "NrMacUe: pduMaker created PDU: " << macPkt->str() << endl;

            // TODO: harq test
            // pdu transmission here (if any)
            // txAcid has HARQ_NONE for non-fillable codeword, acid otherwise
            if (txList.second.empty()) {
                EV << "NrMacUe() : no available process for this MAC pdu in TxHarqBuffer" << endl;
                delete macPkt;
            }
            else {
                //Insert PDU in the Harq Tx Buffer
                //txList.first is the acid
                txBuf->insertPdu(txList.first, cw, macPkt);
            }
        }
    }
}

LteHarqBufferRx *NrMacUeD2D::createRxHarqBuffer(MacNodeId src, const UserControlInfo *userInfo)
{
    Direction dir = (Direction)userInfo->getDirection();
    if (dir == D2D || dir == D2D_MULTI)
        return new LteHarqBufferRxD2D(harqProcesses_, this, binder_, src, (dir == D2D_MULTI));
    return NrMacUe::createRxHarqBuffer(src, userInfo);
}

LteHarqBufferTx *NrMacUeD2D::createTxHarqBuffer(MacNodeId destId, Direction dir)
{
    // NOTE: unlike the base class, the UL buffer is paired with the MAC of destId, not of the serving cell
    if (dir == UL)
        return new LteHarqBufferTx(binder_, (unsigned int)harqProcesses_, this, check_and_cast<LteMacBase *>(binder_->getMacByNodeId(destId)));
    else // D2D or D2D_MULTI
        return new LteHarqBufferTxD2D(binder_, (unsigned int)harqProcesses_, this, check_and_cast<LteMacBase *>(binder_->getMacByNodeId(destId)));
}

LcgScheduler *NrMacUeD2D::createLcgScheduler()
{
    return new LcgSchedulerD2D(this);
}

void NrMacUeD2D::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        NrMacUe::handleMessage(msg);
        return;
    }

    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *incoming = pkt->getArrivalGate();

    if (incoming == downInGate_) {
        auto userInfo = pkt->getTag<UserControlInfo>();

        if (userInfo->getFrameType() == D2DMODESWITCHPKT) {
            EV << "NrMacUeD2D::handleMessage - Received packet " << pkt->getName() <<
                " from port " << pkt->getArrivalGate()->getName() << endl;

            // message from phyIn gate (from the lower layer)
            emit(receivedPacketFromLowerLayerSignal_, pkt);

            // call handler
            macHandleD2DModeSwitch(pkt);

            return;
        }
    }

    NrMacUe::handleMessage(msg);
}

void NrMacUeD2D::macHandleGrant(cPacket *pktAux)
{
    EV << NOW << " NrMacUeD2D::macHandleGrant - UE [" << nodeId_ << "] - Grant received " << endl;

    // extract grant
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto grant = pkt->popAtFront<LteSchedulingGrant>();

    auto userInfo = pkt->getTag<UserControlInfo>();
    GHz carrierFrequency = userInfo->getCarrierFrequency();
    EV << NOW << " NrMacUeD2D::macHandleGrant - Direction: " << dirToA(grant->getDirection()) << " Carrier: " << carrierFrequency << endl;

    // delete old grant
    if (schedulingGrant_.find(carrierFrequency) != schedulingGrant_.end() && schedulingGrant_[carrierFrequency] != nullptr) {
        schedulingGrant_[carrierFrequency] = nullptr;
    }

    // store received grant
    schedulingGrant_[carrierFrequency] = grant;
    if (grant->getPeriodic()) {
        periodCounter_[carrierFrequency] = grant->getPeriod();
        expirationCounter_[carrierFrequency] = grant->getExpiration();
    }

    EV << NOW << " Node " << nodeId_ << " received grant of blocks " << grant->getTotalGrantedBlocks()
       << ", bytes " << grant->getGrantedCwBytes(0) << " Direction: " << dirToA(grant->getDirection()) << endl;

    // clearing pending RAC requests
    racRequested_ = false;
    d2dUeHelper_.setRacD2DMulticastRequested(false);

    delete pkt;
}

void NrMacUeD2D::checkRAC()
{
    EV << NOW << " NrMacUeD2D::checkRAC , Ue  " << nodeId_ << ", racTimer : " << racBackoffTimer_ << " maxRacTryOuts : " << maxRacTryouts_
       << ", raRespTimer:" << raRespTimer_ << endl;

    if (racBackoffTimer_ > 0) {
        racBackoffTimer_--;
        return;
    }

    if (raRespTimer_ > 0) {
        // decrease RAC response timer
        raRespTimer_--;
        EV << NOW << " NrMacUeD2D::checkRAC - waiting for previous RAC requests to complete (timer=" << raRespTimer_ << ")" << endl;
        return;
    }

    if (bsrRtxTimer_ > 0) {
        // decrease BSR timer
        bsrRtxTimer_--;
        EV << NOW << " LteMacUe::checkRAC - waiting for a grant, BSR rtx timer has not expired yet (timer=" << bsrRtxTimer_ << ")" << endl;

        return;
    }

    // Avoids double requests within the same TTI window
    if (racRequested_) {
        EV << NOW << " NrMacUeD2D::checkRAC - double RAC request" << endl;
        racRequested_ = false;
        return;
    }
    if (d2dUeHelper_.getRacD2DMulticastRequested()) {
        EV << NOW << " NrMacUeD2D::checkRAC - double RAC request" << endl;
        d2dUeHelper_.setRacD2DMulticastRequested(false);
        return;
    }

    bool trigger = false;
    bool triggerD2DMulticast = false;

    for (auto [cid, connInfo] : connDescOut_) {
        if (!(connInfo.buffer->isEmpty())) {
            if (connInfo.flowInfo.getDirection() == D2D_MULTI)
                triggerD2DMulticast = true;
            else
                trigger = true;
            break;
        }
    }

    if (!trigger && !triggerD2DMulticast) {
        EV << NOW << " NrMacUeD2D::checkRAC , Ue " << nodeId_ << ", RAC aborted, no data in queues " << endl;
    }

    racRequested_ = trigger;
    bool racD2DMulticastRequested = d2dUeHelper_.getRacD2DMulticastRequested();
    if (!racRequested_)
        racD2DMulticastRequested = triggerD2DMulticast;
    d2dUeHelper_.setRacD2DMulticastRequested(racD2DMulticastRequested);
    if (racRequested_ || racD2DMulticastRequested) {
        auto pkt = new Packet("RacRequest");
        GHz carrierFrequency = phy_->getPrimaryChannelModel()->getCarrierFrequency();
        pkt->addTagIfAbsent<UserControlInfo>()->setCarrierFrequency(carrierFrequency);
        pkt->addTagIfAbsent<UserControlInfo>()->setSourceId(getMacNodeId());
        pkt->addTagIfAbsent<UserControlInfo>()->setDestId(getMacCellId());
        pkt->addTagIfAbsent<UserControlInfo>()->setDirection(UL);
        pkt->addTagIfAbsent<UserControlInfo>()->setFrameType(RACPKT);

        auto racReq = makeShared<LteRac>();
        racReq->setPreambleIndex(intuniform(0, numPreambles_ - 1));

        pkt->insertAtFront(racReq);
        sendLowerPackets(pkt);

        EV << NOW << " Ue  " << nodeId_ << " cell " << cellId_ << ", RAC request sent to PHY (preamble="
           << racReq->getPreambleIndex() << ")" << endl;

        // wait at least  "raRespWinStart_" TTIs before another RAC request
        raRespTimer_ = raRespWinStart_;
    }
}

void NrMacUeD2D::macHandleRac(cPacket *pktAux)
{
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto racPkt = pkt->peekAtFront<LteRac>();

    if (racPkt->getSuccess()) {
        EV << "NrMacUeD2D::macHandleRac - Ue " << nodeId_ << " won RAC" << endl;
        // if RAC is won, BSR has to be sent
        if (d2dUeHelper_.getRacD2DMulticastRequested())
            d2dUeHelper_.setBsrD2DMulticastTriggered(true);
        else
            bsrTriggered_ = true;

        // reset RAC counter
        currentRacTry_ = 0;
        //reset RAC backoff timer
        racBackoffTimer_ = 0;
    }
    else {
        // RAC has failed
        if (++currentRacTry_ >= maxRacTryouts_) {
            EV << NOW << " Ue " << nodeId_ << ", RAC reached max attempts : " << currentRacTry_ << endl;
            // no more RAC allowed
            //! TODO flush all buffers here
            //reset RAC counter
            currentRacTry_ = 0;
            //reset RAC backoff timer
            racBackoffTimer_ = 0;
        }
        else {
            // recompute backoff timer
            racBackoffTimer_ = uniform(minRacBackoff_, maxRacBackoff_);
            EV << NOW << " Ue " << nodeId_ << " RAC attempt failed, backoff extracted : " << racBackoffTimer_ << endl;
        }
    }
    delete pkt;
}

void NrMacUeD2D::macHandleD2DModeSwitch(cPacket *pkt)
{
    d2dUeHelper_.macHandleD2DModeSwitch(pkt);
}

void NrMacUeD2D::doHandover(MacNodeId targetEnb)
{
    if (targetEnb == NODEID_NONE)
        d2dUeHelper_.setEnb(nullptr);
    else {
        d2dUeHelper_.rebuildPreconfiguredTxParams(binder_);
        d2dUeHelper_.setEnb(check_and_cast<ID2dMacEnb *>(binder_->getMacByNodeId(targetEnb)));
    }
    NrMacUe::doHandover(targetEnb);
}

} //namespace
