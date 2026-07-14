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

#ifndef _SIDELINK_SLHARQTXENTITY_H_
#define _SIDELINK_SLHARQTXENTITY_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * Sidelink TX HARQ entity (design decision D2, WP-F): 16 processes, blind
 * retransmissions only (SL-1; PSFCH-based feedback arrives with SL-2).
 * Written fresh rather than reusing the feedback-driven Uu HARQ buffers
 * (gap G5): a blind-retx process has no state machine, only a copy of the
 * TB and a remaining-copies counter.
 *
 * SL-1 simplification (per the plan): the retransmission copies go out on
 * the grant's reserved future resources (the next period slots of the same
 * selected resource train); the spec's up-to-32-resource chains within one
 * period are not modeled.
 */
class SlHarqTxEntity
{
  public:
    static constexpr int NUM_PROCESSES = 16;

    struct Retx {
        inet::Packet *pdu = nullptr;   // copy to transmit (caller takes ownership)
        int procId = 0;
        bool ndi = false;
        int rv = 0;                    // 1 for the first blind copy, 2 for the second, ...
    };

  private:
    struct TxProcess {
        inet::Packet *pdu = nullptr;   // stored TB for pending blind copies
        int remainingRetx = 0;
        int txCount = 0;               // transmissions done so far (1 after the initial TX)
        bool ndi = false;
    };

    TxProcess processes_[NUM_PROCESSES];
    int nextProcess_ = 0;

  public:
    ~SlHarqTxEntity();

    /// register a new TB about to be transmitted; stores a copy if blind
    /// retransmissions are configured. Returns the assigned process id and
    /// sets ndi (toggled per new TB on the process).
    int startTb(const inet::Packet *pdu, int numBlindRetx, bool& ndi);

    bool hasPendingRetx() const;

    /// pop the next blind copy to transmit (nullptr if none); the caller
    /// owns the returned packet
    bool getNextRetx(Retx& out);
};

} // namespace simu5g

#endif
