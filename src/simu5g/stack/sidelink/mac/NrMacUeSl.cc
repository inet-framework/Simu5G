//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/mac/NrMacUeSl.h"

#include "simu5g/stack/mac/buffer/LteMacBuffer.h"
#include "simu5g/stack/mac/buffer/harq/LteHarqBufferTx.h"
#include "simu5g/stack/mac/packet/LteMacPdu.h"
#include "simu5g/stack/mac/packet/LteRac_m.h"
#include "simu5g/stack/mac/packet/LteSchedulingGrant.h"
#include "simu5g/stack/phy/PhyBase.h"
#include "simu5g/stack/sidelink/mac/NrSlMacUe.h"
#include "simu5g/stack/sidelink/mac/SlSchedulingGrant_m.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrMacUeSl);

void NrMacUeSl::initialize(int stage)
{
    NrMacUe::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        // tolerate a custom SL MAC type: the mode-1 bridge is off then
        slMac_ = dynamic_cast<NrSlMacUe *>(getModuleByPath(par("slMacModule").stringValue()));
    }
}

void NrMacUeSl::checkRAC()
{
    // mirror of LteMacUe::checkRAC (the LteMacUeD2D precedent) with the SL
    // trigger added after the Uu-buffer scan - Uu wins ties (D26)
    EV << NOW << " NrMacUeSl::checkRAC , UE  " << nodeId_ << ", racTimer : " << racBackoffTimer_
       << " maxRacTryOuts : " << maxRacTryouts_ << ", raRespTimer:" << raRespTimer_ << endl;

    if (racBackoffTimer_ > 0) {
        racBackoffTimer_--;
        return;
    }

    if (raRespTimer_ > 0) {
        // decrease RAC response timer
        raRespTimer_--;
        EV << NOW << " NrMacUeSl::checkRAC - waiting for previous RAC requests to complete (timer=" << raRespTimer_ << ")" << endl;
        return;
    }

    if (bsrRtxTimer_ > 0) {
        // decrease BSR timer
        bsrRtxTimer_--;
        EV << NOW << " NrMacUeSl::checkRAC - waiting for a grant, BSR RTX timer has not expired yet (timer=" << bsrRtxTimer_ << ")" << endl;
        return;
    }

    // avoids double requests within the same TTI window
    if (racRequested_) {
        EV << NOW << " NrMacUeSl::checkRAC - double RAC request" << endl;
        racRequested_ = false;
        return;
    }
    if (slRacRequested_) {
        EV << NOW << " NrMacUeSl::checkRAC - double RAC request (SL)" << endl;
        slRacRequested_ = false;
        return;
    }

    bool uuTrigger = false;
    for (const auto& it : connDescOut_) {
        if (!(it.second.buffer->isEmpty())) {
            uuTrigger = true;
            break;
        }
    }

    // the SL trigger: mode-1 backlog with no active/pending grant; a Uu
    // request always takes precedence (the SL side retries next cycle)
    bool slTrigger = !uuTrigger && slMac_ != nullptr && slMac_->mode1BsrPending();

    if (!uuTrigger && !slTrigger)
        EV << NOW << "UE " << nodeId_ << ", RAC aborted, no data in queues " << endl;

    racRequested_ = uuTrigger;
    slRacRequested_ = slTrigger;

    if (uuTrigger || slTrigger) {
        auto pkt = new Packet("RacRequest");

        auto racReq = makeShared<LteRac>();
        racReq->setPreambleIndex(intuniform(0, numPreambles_ - 1));
        pkt->insertAtFront(racReq);

        GHz carrierFrequency = phy_->getPrimaryChannelModel()->getCarrierFrequency();
        pkt->addTagIfAbsent<UserControlInfo>()->setCarrierFrequency(carrierFrequency);
        pkt->addTagIfAbsent<UserControlInfo>()->setSourceId(getMacNodeId());
        pkt->addTagIfAbsent<UserControlInfo>()->setDestId(getMacCellId());
        pkt->addTagIfAbsent<UserControlInfo>()->setDirection(UL);
        pkt->addTagIfAbsent<UserControlInfo>()->setFrameType(RACPKT);

        sendLowerPackets(pkt);

        EV << NOW << " UE  " << nodeId_ << " cell " << cellId_ << " ,RAC request sent to PHY (preamble="
           << racReq->getPreambleIndex() << (slTrigger ? ", SL-triggered" : "") << ")" << endl;

        if (slTrigger)
            slMac_->onMode1RequestStarted();  // grant-cycle latency anchor

        // wait at least "raRespWinStart_" TTIs before another RAC request
        raRespTimer_ = raRespWinStart_;
    }
}

void NrMacUeSl::macHandleRac(cPacket *pktAux)
{
    // mirror of LteMacUe::macHandleRac with the SL branch (D26): a won RAC
    // that was SL-triggered arms the SL-BSR instead of the Uu one
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto racPkt = pkt->peekAtFront<LteRac>();

    if (racPkt->getSuccess()) {
        EV << "NrMacUeSl::macHandleRac - UE " << nodeId_ << " won RAC"
           << (slRacRequested_ ? " (SL-triggered)" : "") << endl;

        if (slRacRequested_) {
            slBsrTriggered_ = true;
            slRacRequested_ = false;
        }
        else {
            bsrTriggered_ = true;
        }
        // reset RAC counter and backoff timer
        currentRacTry_ = 0;
        racBackoffTimer_ = 0;
    }
    else {
        // RAC has failed
        if (++currentRacTry_ >= maxRacTryouts_) {
            EV << NOW << " UE " << nodeId_ << ", RAC reached max attempts : " << currentRacTry_ << endl;
            // no more RAC allowed
            currentRacTry_ = 0;
            racBackoffTimer_ = 0;
            slRacRequested_ = false;
        }
        else {
            // recompute backoff timer
            racBackoffTimer_ = uniform(minRacBackoff_, maxRacBackoff_);
            EV << NOW << " UE " << nodeId_ << " RAC attempt failed, backoff extracted : " << racBackoffTimer_ << endl;
        }
    }
    delete pkt;
}

void NrMacUeSl::macHandleGrant(cPacket *pktAux)
{
    // an SL grant (D28) is routed to the SL MAC and never touches the base
    // Uu grant state (G22: an unintercepted SlSchedulingGrant would start
    // driving Uu UL PDU making)
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto slGrant = dynamicPtrCast<const SlSchedulingGrant>(pkt->peekAtFront<LteSchedulingGrant>());
    if (slGrant != nullptr) {
        EV << NOW << " NrMacUeSl::macHandleGrant - UE [" << nodeId_ << "] - SL grant received, routing to slMac" << endl;
        if (slMac_ == nullptr)
            throw cRuntimeError("NrMacUeSl: received an SlSchedulingGrant but the SL MAC is not an NrSlMacUe");
        slMac_->onMode1Grant(slGrant.get());
        // the pending RAC/BSR cycle is complete: clear the request guard AND
        // the BSR retransmission timer - its purpose is to cover a lost
        // grant, and unlike the Uu eNB (which keeps a standing backlog view
        // from BSRs) the SL scheduler is purely reactive, so leftover
        // backlog must be free to start the next RAC/BSR cycle immediately
        racRequested_ = false;
        bsrRtxTimer_ = 0;
        delete pkt;
        return;
    }

    NrMacUe::macHandleGrant(pktAux);
}

bool NrMacUeSl::makeSlBsrPdu(GHz carrierFreq)
{
    int sizeBsr = slMac_ != nullptr ? slMac_->mode1BsrBytes() : 0;
    if (sizeBsr <= 0) {
        // the SL backlog vanished (e.g. the grant arrived meanwhile): clear
        slBsrTriggered_ = false;
        bsrRtxTimer_ = 0;
        return false;
    }

    // the D2dUeMacHelper::makeBsr pattern: a BSR-only MAC PDU whose channel
    // identity travels on UserControlInfo::packetLcid (D26)
    auto macPkt = new Packet("SlBsrPdu");
    auto header = makeShared<LteMacPdu>();
    header->setHeaderLength(MAC_HEADER);
    macPkt->setTimestamp(NOW);

    MacBsr *bsr = new MacBsr();
    bsr->setTimestamp(simTime().dbl());
    bsr->setSize(sizeBsr);
    header->pushCe(bsr);
    macPkt->insertAtFront(header);

    auto info = macPkt->addTagIfAbsent<UserControlInfo>();
    info->setSourceId(getMacNodeId());
    info->setDestId(getMacCellId());
    info->setDirection(UL);
    info->setPacketLcid(SL_SHORT_BSR);
    info->setCarrierFrequency(carrierFreq);
    info->setUserTxParams(schedulingGrant_[carrierFreq]->getUserTxParams()->dup());

    slBsrTriggered_ = false;
    bsrRtxTimer_ = bsrRtxTimerStart_;  // prevents an unnecessary new RAC while the grant is pending

    // enqueue on the Uu UL HARQ machinery like any UL PDU (the base
    // macPduMake tail-end, reduced to the single BSR PDU)
    if (harqTxBuffers_.find(carrierFreq) == harqTxBuffers_.end())
        harqTxBuffers_[carrierFreq] = HarqTxBuffers();
    HarqTxBuffers& harqTxBuffers = harqTxBuffers_[carrierFreq];

    LteHarqBufferTx *txBuf;
    auto hit = harqTxBuffers.find(cellId_);
    if (hit != harqTxBuffers.end()) {
        txBuf = hit->second;
    }
    else {
        txBuf = createTxHarqBuffer(cellId_, UL);
        harqTxBuffers[cellId_] = txBuf;
    }

    UnitList txList = txBuf->firstAvailable();
    if (txList.second.empty()) {
        EV << NOW << " NrMacUeSl::makeSlBsrPdu - no available HARQ process, dropping the SL-BSR" << endl;
        delete macPkt;
        return false;
    }
    txBuf->insertPdu(txList.first, txList.second.front(), macPkt);

    EV << NOW << " NrMacUeSl::makeSlBsrPdu - SL-BSR of " << sizeBsr << "B created (carrier "
       << carrierFreq << ")" << endl;
    return true;
}

void NrMacUeSl::macPduMake(MacCid cid)
{
    // the SL-BSR branch (D26): a UL grant with an empty UL schedule list is
    // the BSR opportunity - the Uu BSR (base behavior) wins ties, the SL-BSR
    // uses the grant only when no Uu BSR is pending
    if (slBsrTriggered_ && !bsrTriggered_ && emptyScheduleList_) {
        for (auto& [carrierFreq, grant] : schedulingGrant_) {
            // skip if this is not the turn of this carrier
            if (getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq((carrierFreq))) > 0)
                continue;

            if (grant != nullptr && grant->getDirection() == UL) {
                macPduList_.clear();
                makeSlBsrPdu(carrierFreq);
                return;  // the grant is spent on the SL-BSR (or the trigger cleared)
            }
        }
    }

    NrMacUe::macPduMake(cid);
}

} // namespace simu5g
