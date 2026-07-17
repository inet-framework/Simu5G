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

#include "simu5g/stack/sidelink/mac/SlHarqTxEntity.h"

namespace simu5g {

SlHarqTxEntity::~SlHarqTxEntity()
{
    for (auto& p : processes_)
        delete p.pdu;
}

void SlHarqTxEntity::freeProcess(TxProcess& p)
{
    delete p.pdu;
    p.pdu = nullptr;
    p.remainingRetx = 0;
    p.feedbackMode = false;
    p.awaitingFeedback = false;
    p.feedbackDeadline = SLOTINDEX_NONE;
    p.dstPid = NODEID_NONE;
    p.nackOnly = false;
    p.expectedAcks = 0;
    p.ackCount = 0;
    p.nackSeen = false;
    p.retxQueued = false;
}

void SlHarqTxEntity::nackProcess(TxProcess& p, int maxRtx)
{
    if (p.txCount > maxRtx) {
        // retransmission budget exhausted: give the TB up
        numGivenUp_++;
        freeProcess(p);
        return;
    }
    p.awaitingFeedback = false;
    p.retxQueued = true;
}

int SlHarqTxEntity::startTb(const inet::Packet *pdu, int numBlindRetx, bool& ndi)
{
    int procId = nextProcess_;
    nextProcess_ = (nextProcess_ + 1) % NUM_PROCESSES;

    TxProcess& p = processes_[procId];
    // an overwritten process drops its remaining copies / awaited feedback
    if (p.busy())
        numGivenUp_++;
    bool prevNdi = p.ndi;
    freeProcess(p);
    p.pdu = (numBlindRetx > 0) ? pdu->dup() : nullptr;
    p.remainingRetx = numBlindRetx;
    p.txCount = 1;
    p.ndi = !prevNdi;

    ndi = p.ndi;
    return procId;
}

int SlHarqTxEntity::startTbWithFeedback(const inet::Packet *pdu, MacNodeId dstPid,
        SlotIndex feedbackDeadline, bool nackOnly, int expectedAcks, bool& ndi)
{
    // prefer a free process (feedback processes stay busy for a while);
    // fall back to overwriting the round-robin slot
    int procId = -1;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int candidate = (nextProcess_ + i) % NUM_PROCESSES;
        if (!processes_[candidate].busy()) {
            procId = candidate;
            break;
        }
    }
    if (procId < 0) {
        procId = nextProcess_;
        numGivenUp_++;
    }
    nextProcess_ = (procId + 1) % NUM_PROCESSES;

    TxProcess& p = processes_[procId];
    bool prevNdi = p.ndi;
    freeProcess(p);
    p.pdu = pdu->dup();
    p.txCount = 1;
    p.ndi = !prevNdi;
    p.feedbackMode = true;
    p.awaitingFeedback = true;
    p.feedbackDeadline = feedbackDeadline;
    p.dstPid = dstPid;
    p.nackOnly = nackOnly;
    p.expectedAcks = expectedAcks;

    ndi = p.ndi;
    return procId;
}

void SlHarqTxEntity::onFeedback(int procId, MacNodeId fbSender, bool ack, int maxRtx)
{
    if (procId < 0 || procId >= NUM_PROCESSES)
        return;
    TxProcess& p = processes_[procId];
    if (!p.feedbackMode || !p.awaitingFeedback)
        return;  // stale or unexpected feedback
    // for unicast the acknowledged peer is the destination itself; groupcast
    // destinations are pseudo ids, any member's feedback counts
    if (p.expectedAcks == 1 && p.dstPid != fbSender)
        return;

    if (!ack) {
        p.nackSeen = true;
        nackProcess(p, maxRtx);
        return;
    }

    p.ackCount++;
    if (!p.nackOnly && p.ackCount >= p.expectedAcks && !p.nackSeen)
        freeProcess(p);  // fully acknowledged
}

int SlHarqTxEntity::processDeadlines(SlotIndex currentSlot, bool dtxAsAck, int maxRtx)
{
    int numDtx = 0;
    for (auto& p : processes_) {
        if (!p.awaitingFeedback || p.feedbackDeadline > currentSlot)
            continue;

        if (p.nackOnly) {
            // groupcast option 1: silence past the deadline means success
            freeProcess(p);
        }
        else {
            numDtx++;
            if (dtxAsAck)
                freeProcess(p);
            else
                nackProcess(p, maxRtx);
        }
    }
    return numDtx;
}

void SlHarqTxEntity::rearmFeedback(int procId, SlotIndex feedbackDeadline)
{
    TxProcess& p = processes_[procId];
    ASSERT(p.feedbackMode && !p.awaitingFeedback);
    p.awaitingFeedback = true;
    p.feedbackDeadline = feedbackDeadline;
    p.ackCount = 0;
    p.nackSeen = false;
}

bool SlHarqTxEntity::hasPendingRetx() const
{
    for (const auto& p : processes_)
        if (p.remainingRetx > 0 || p.retxQueued)
            return true;
    return false;
}

bool SlHarqTxEntity::hasAwaitingFeedback() const
{
    for (const auto& p : processes_)
        if (p.awaitingFeedback)
            return true;
    return false;
}

bool SlHarqTxEntity::getNextRetx(Retx& out)
{
    for (int i = 0; i < NUM_PROCESSES; i++) {
        TxProcess& p = processes_[i];

        if (p.remainingRetx > 0) {
            // blind copy
            out.procId = i;
            out.ndi = p.ndi;
            out.rv = p.txCount;   // 1 for the first blind copy, ...
            out.feedbackMode = false;
            out.pdu = (p.remainingRetx == 1) ? p.pdu : p.pdu->dup();
            if (p.remainingRetx == 1)
                p.pdu = nullptr;  // last copy: hand over the stored TB itself
            p.remainingRetx--;
            p.txCount++;
            return true;
        }

        if (p.retxQueued) {
            // NACK'd (or DTX'd) feedback-mode copy; the TB stays stored for
            // possible further retransmissions, the caller re-arms the wait
            out.procId = i;
            out.ndi = p.ndi;
            out.rv = p.txCount;
            out.feedbackMode = true;
            out.pdu = p.pdu->dup();
            p.retxQueued = false;
            p.txCount++;
            return true;
        }
    }
    return false;
}

} // namespace simu5g
