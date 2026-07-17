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
 * Sidelink TX HARQ entity (design decisions D2/D24): 16 processes, written
 * fresh rather than reusing the Uu HARQ buffers (gap G5). Two per-TB modes:
 *
 *  - blind (WP-F, SL-1): a fixed number of copies follows the initial TX on
 *    the grant train; no state machine, only the stored TB and a counter.
 *    Used by broadcast and by any destination without PSFCH.
 *
 *  - feedback-driven (WP-I, D24): the transmitted TB parks awaiting PSFCH;
 *    NACK queues a retransmission copy (served by the existing
 *    retx-before-new-data occasion logic), ACK frees the process, and
 *    feedback missing past the deadline (DTX -- e.g. half-duplex loss of the
 *    PSFCH slot) is resolved by the caller's policy (treat as NACK, up to
 *    maxRtx, or as ACK). Groupcast option 1 TBs are NACK-only: silence at
 *    the deadline always means success; option 2 TBs count per-member ACKs
 *    (all expected ACKs received frees the process early).
 *
 * SL-1 simplification kept: retransmission copies ride the same selected
 * resource train (the grant's next period slots).
 */
class SlHarqTxEntity
{
  public:
    static constexpr int NUM_PROCESSES = 16;

    struct Retx {
        inet::Packet *pdu = nullptr;   // copy to transmit (caller takes ownership)
        int procId = 0;
        bool ndi = false;
        int rv = 0;                    // 1 for the first retransmission, 2 for the second, ...
        bool feedbackMode = false;     // caller must re-arm the feedback deadline after TX
    };

  private:
    struct TxProcess {
        inet::Packet *pdu = nullptr;   // stored TB for pending copies / awaited feedback
        int remainingRetx = 0;         // blind mode: copies still to send
        int txCount = 0;               // transmissions done so far (1 after the initial TX)
        bool ndi = false;

        // feedback mode (D24)
        bool feedbackMode = false;
        bool awaitingFeedback = false;
        SlotIndex feedbackDeadline = SLOTINDEX_NONE;
        MacNodeId dstPid = NODEID_NONE;
        bool nackOnly = false;         // groupcast option 1: DTX at deadline = success
        int expectedAcks = 0;          // 1 unicast; group size - 1 for option 2
        int ackCount = 0;
        bool nackSeen = false;
        bool retxQueued = false;       // a NACK'd copy waits for a TX occasion

        bool busy() const { return remainingRetx > 0 || awaitingFeedback || retxQueued; }
    };

    TxProcess processes_[NUM_PROCESSES];
    int nextProcess_ = 0;
    unsigned int numGivenUp_ = 0;      // TBs dropped at maxRtx or by process overwrite

    /// release the process (drops the stored TB); keeps ndi
    void freeProcess(TxProcess& p);

    /// NACK-equivalent event: queue a copy, or give the TB up at maxRtx
    void nackProcess(TxProcess& p, int maxRtx);

  public:
    ~SlHarqTxEntity();

    /// register a new blind-mode TB about to be transmitted; stores a copy
    /// if blind retransmissions are configured. Returns the assigned process
    /// id and sets ndi (toggled per new TB on the process).
    int startTb(const inet::Packet *pdu, int numBlindRetx, bool& ndi);

    /// register a new feedback-mode TB about to be transmitted (D24): the
    /// process parks awaiting PSFCH until feedbackDeadline (an absolute slot
    /// index). Prefers a free process; overwrites (gives up) the round-robin
    /// slot if all 16 are busy.
    int startTbWithFeedback(const inet::Packet *pdu, MacNodeId dstPid, SlotIndex feedbackDeadline,
            bool nackOnly, int expectedAcks, bool& ndi);

    /// decoded PSFCH input for one of our processes; ignores stale feedback
    void onFeedback(int procId, MacNodeId fbSender, bool ack, int maxRtx);

    /// sweep feedback deadlines at a TX occasion; DTX resolves per dtxAsAck
    /// (nack-only TBs always resolve to success). Returns the number of
    /// DTX events.
    int processDeadlines(SlotIndex currentSlot, bool dtxAsAck, int maxRtx);

    /// re-arm the feedback wait after transmitting a feedback-mode copy
    void rearmFeedback(int procId, SlotIndex feedbackDeadline);

    bool hasPendingRetx() const;

    /// any feedback-mode process still awaiting PSFCH (the caller must keep
    /// TX occasions scheduled so deadlines are checked)
    bool hasAwaitingFeedback() const;

    /// pop the next copy to transmit (blind or NACK'd); the caller owns the
    /// returned packet
    bool getNextRetx(Retx& out);

    unsigned int getNumGivenUp() const { return numGivenUp_; }
};

} // namespace simu5g

#endif
