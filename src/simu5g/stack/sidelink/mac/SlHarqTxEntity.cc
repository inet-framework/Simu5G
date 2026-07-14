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

int SlHarqTxEntity::startTb(const inet::Packet *pdu, int numBlindRetx, bool& ndi)
{
    int procId = nextProcess_;
    nextProcess_ = (nextProcess_ + 1) % NUM_PROCESSES;

    TxProcess& p = processes_[procId];
    // an overwritten process simply drops its remaining blind copies
    delete p.pdu;
    p.pdu = (numBlindRetx > 0) ? pdu->dup() : nullptr;
    p.remainingRetx = numBlindRetx;
    p.txCount = 1;
    p.ndi = !p.ndi;

    ndi = p.ndi;
    return procId;
}

bool SlHarqTxEntity::hasPendingRetx() const
{
    for (const auto& p : processes_)
        if (p.remainingRetx > 0)
            return true;
    return false;
}

bool SlHarqTxEntity::getNextRetx(Retx& out)
{
    for (int i = 0; i < NUM_PROCESSES; i++) {
        TxProcess& p = processes_[i];
        if (p.remainingRetx <= 0)
            continue;

        out.procId = i;
        out.ndi = p.ndi;
        out.rv = p.txCount;   // 1 for the first blind copy, ...
        out.pdu = (p.remainingRetx == 1) ? p.pdu : p.pdu->dup();
        if (p.remainingRetx == 1)
            p.pdu = nullptr;  // last copy: hand over the stored TB itself
        p.remainingRetx--;
        p.txCount++;
        return true;
    }
    return false;
}

} // namespace simu5g
