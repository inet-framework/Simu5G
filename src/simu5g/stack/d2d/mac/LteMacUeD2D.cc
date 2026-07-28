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

// NOTE: registered HERE (not in the mixin or the helper) to keep the global
// signal registration order -- and thus the sz fingerprint -- unchanged.
simsignal_t LteMacUeD2D::rcvdD2DModeSwitchNotificationSignal_ = registerSignal("rcvdD2DModeSwitchNotification");

LteMacUeD2D::LteMacUeD2D() : D2dUeMacBase<LteMacUe>(rcvdD2DModeSwitchNotificationSignal_)
{
}

void LteMacUeD2D::handleSelfMessage()
{
    EV << "----- UE MAIN LOOP -----" << endl;

    // extract PDUs from all HARQ RX buffers and pass them to unmaker
    for (auto& [carrierFreq, harqRxMap] : harqRxBuffers_) {
        for (auto& [nodeId, rxBuffer] : harqRxMap) {
            std::list<Packet *> pduList = rxBuffer->extractCorrectPdus();
            while (!pduList.empty()) {
                auto pdu = pduList.front();
                pduList.pop_front();
                macPduUnmake(pdu);
            }
        }
    }

    EV << NOW << " LteMacUeD2D::handleSelfMessage " << nodeId_ << " - HARQ process " << (unsigned int)currentHarq_ << endl;

    // no grant available - if user has backlogged data, it will trigger scheduling request
    // no HARQ counter is updated since no transmission is sent.

    bool noSchedulingGrants = true;
    for (const auto& git : schedulingGrant_) {
        if (git.second != nullptr)
            noSchedulingGrants = false;
    }

    if (noSchedulingGrants) {
        EV << NOW << " LteMacUe::handleSelfMessage " << nodeId_ << " NO configured grant" << endl;

        // if necessary, a RAC request will be sent to obtain a grant
        checkRAC();
        // TODO ensure all operations done before return (i.e. move H-ARQ RX purge before this point)
    }
    else {
        bool periodicGrant = false;
        bool checkRac = false;
        bool skip = false;
        for (auto& git : schedulingGrant_) {
            if (git.second != nullptr && git.second->getPeriodic()) {
                periodicGrant = true;
                GHz carrierFreq = git.first;

                // Periodic checks
                if (--expirationCounter_[carrierFreq] < 0) {
                    // Periodic grant is expired
                    git.second = nullptr;
                    // if necessary, a RAC request will be sent to obtain a grant
                    checkRac = true;
                }
                else if (--periodCounter_[carrierFreq] > 0) {
                    skip = true;
                }
                else {
                    // resetting grant period
                    periodCounter_[carrierFreq] = git.second->getPeriod();
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
        if (!firstTx) {
            EV << "\t currentHarq_ counter initialized " << endl;
            firstTx = true;
            // the eNB will receive the first PDU in 2 TTI, thus initializing acid to 0
            currentHarq_ = harqProcesses_ - 2;
        }

        EV << NOW << " LteMacUeD2D::handleSelfMessage " << nodeId_ << " entered scheduling" << endl;

        bool retx = false;

        LteHarqBufferTx *currHarq;
        for (auto& [carrierFrequency, harqTxMap] : harqTxBuffers_) {
            // skip if no grant is configured for this carrier
            if (schedulingGrant_.find(carrierFrequency) == schedulingGrant_.end() || schedulingGrant_[carrierFrequency] == nullptr)
                continue;

            for (auto& [nodeId, harqBuffer] : harqTxMap) {
                EV << "\t Looking for retx in acid " << (unsigned int)currentHarq_ << endl;
                currHarq = harqBuffer;

                // check if the current process has unit ready for retx
                bool ready = currHarq->getProcess(currentHarq_)->hasReadyUnits();
                CwList cwListRetx = currHarq->getProcess(currentHarq_)->readyUnitsIds();

                EV << "\t [process=" << (unsigned int)currentHarq_ << "] , [retx=" << (ready ? "true" : "false")
                   << "] , [n=" << cwListRetx.size() << "]" << endl;

                // check if one 'ready' unit has the same direction as the grant
                bool checkDir = false;
                for (Codeword cw : cwListRetx) {
                    auto info = currHarq->getProcess(currentHarq_)->getPdu(cw)->getTag<UserControlInfo>();
                    if (info->getDirection() == schedulingGrant_[carrierFrequency]->getDirection()) {
                        checkDir = true;
                        break;
                    }
                }

                // if a retransmission is needed
                if (ready && checkDir) {
                    UnitList signal;
                    signal.first = currentHarq_;
                    signal.second = cwListRetx;
                    currHarq->markSelected(signal, schedulingGrant_[carrierFrequency]->getUserTxParams()->getLayers().size());
                    retx = true;
                }
            }
        }
        // if no retx is needed, proceed with normal scheduling
        if (!retx) {
            emptyScheduleList_ = true;
            std::map<GHz, LteSchedulerUeUl *>::iterator sit;
            for (auto [carrierFrequency, carrierLcgScheduler] : lcgScheduler_) {
                EV << "LteMacUeD2D::handleSelfMessage - running LCG scheduler for carrier [" << carrierFrequency << "]" << endl;
                LteMacScheduleList *carrierScheduleList = carrierLcgScheduler->schedule();
                EV << "LteMacUeD2D::handleSelfMessage - scheduled " << carrierScheduleList->size() << " connections on carrier " << carrierFrequency << endl;
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
        for (const auto& [carrierFreq, harqTxMap] : harqTxBuffers_) {
            EV << "\n carrier[ " << carrierFreq << "] htxbuf.size " << harqTxMap.size() << endl;

            EV << "\n htxbuf.size " << harqTxBuffers_.size() << endl;

            int cntOuter = 0;
            int cntInner = 0;
            for (auto [nodeId, currHarq] : harqTxMap) {
                BufferStatus harqStatus = currHarq->getBufferStatus();
                EV << "\t cicloOuter " << cntOuter << " - bufferStatus.size=" << harqStatus.size() << endl;
                for (const auto& jt : harqStatus) {
                    EV << "\t\t cicloInner " << cntInner << " - jt->size=" << jt.size()
                       << " - statusCw(0/1)=" << jt.at(0).second << "/" << jt.at(1).second << endl;
                }
            }
        }
    }
    //======================== END DEBUG ==========================

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
    EV << NOW << " LteMacUeD2D::handleSelfMessage Purged " << purged << " PDUs" << endl;

    if (requestedSdus_ == 0) {
        // update current HARQ process ID
        currentHarq_ = (currentHarq_ + 1) % harqProcesses_;
    }
    EV << "--- END UE MAIN LOOP ---" << endl;
}

} //namespace
