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

#ifndef _SIDELINK_SLUERADIOSTATE_H_
#define _SIDELINK_SLUERADIOSTATE_H_

#include <deque>

#include <omnetpp.h>

namespace simu5g {

/**
 * Per-UE shared radio state for the opt-in Uu/SL half-duplex arbiter
 * (design decision D32, SL-3): a pure object recording the TX intervals of
 * both radio legs of a UE - the Uu leg (NrPhyUeSl) and the SL leg
 * (NrSlPhyUe) - so each leg can ask whether the *other* leg was
 * transmitting during a reception interval. Models the single-radio
 * constraint that the independent legs otherwise ignore; applies
 * regardless of carrier equality (one radio cannot TX at 5.9 GHz and RX
 * at 2 GHz either).
 *
 * Created by SlRrc, registered in SlBinder by node id. Intervals are kept
 * in a bounded deque and pruned lazily on insert (no ticker): queries only
 * ever look back about one slot, so anything older than the horizon is
 * dropped.
 *
 * TX-TX conflicts (both legs transmitting in overlapping intervals) are
 * counted but not suppressed in SL-3 (resolving them needs a TX-defer
 * policy that belongs with sync modeling in SL-4).
 */
class SlUeRadioState
{
  public:
    enum Leg { UU, SL };

  private:
    struct TxInterval
    {
        Leg leg;
        omnetpp::simtime_t start;
        omnetpp::simtime_t end;
    };

    std::deque<TxInterval> intervals_;
    omnetpp::simtime_t pruneHorizon_;
    unsigned int txConflicts_ = 0;

  public:
    explicit SlUeRadioState(omnetpp::simtime_t pruneHorizon = omnetpp::SimTime(10, omnetpp::SIMTIME_MS))
        : pruneHorizon_(pruneHorizon) {}

    /// record a transmission of one leg; returns true if it overlaps a
    /// recorded TX of the *other* leg (a counted TX-TX conflict)
    bool recordTx(Leg leg, omnetpp::simtime_t start, omnetpp::simtime_t end)
    {
        // lazy prune: queries look back at most ~one slot
        while (!intervals_.empty() && intervals_.front().end < start - pruneHorizon_)
            intervals_.pop_front();

        bool conflict = overlapsTx(leg == UU ? SL : UU, start, end);
        if (conflict)
            txConflicts_++;
        intervals_.push_back({ leg, start, end });
        return conflict;
    }

    /// does any recorded TX of the given leg overlap (start, end) with
    /// positive measure? Touching endpoints do NOT count: a TX ending
    /// exactly at a slot boundary does not block the next slot's reception
    /// (back-to-back slots are the normal slotted operation)
    bool overlapsTx(Leg leg, omnetpp::simtime_t start, omnetpp::simtime_t end) const
    {
        for (const auto& iv : intervals_)
            if (iv.leg == leg && iv.start < end && start < iv.end)
                return true;
        return false;
    }

    unsigned int getTxConflicts() const { return txConflicts_; }
    size_t recordedCount() const { return intervals_.size(); }
};

} // namespace simu5g

#endif
