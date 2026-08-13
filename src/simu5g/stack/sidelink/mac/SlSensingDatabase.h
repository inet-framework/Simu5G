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

#ifndef _SIDELINK_SLSENSINGDATABASE_H_
#define _SIDELINK_SLSENSINGDATABASE_H_

#include <deque>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * One decoded SCI-1A observation: the sender's resource usage and its
 * signaled future reservation, with the measured SL-RSRP.
 */
struct SlSensingEntry
{
    SlotIndex slot = SLOTINDEX_NONE;   // slot the SCI was received in
    int firstSubchannel = 0;
    int numSubchannels = 1;
    double rsrpDbm = 0;                // SL-RSRP measured on the PSCCH/PSSCH
    int reservationPeriodSlots = 0;    // signaled resource reservation period (0 = no reservation)
    MacNodeId srcNodeId = NODEID_NONE;
    int priority = 0;                  // SCI priority field (constant in SL-1)
};

/**
 * Per-UE sensing database (design decision and WP-E step 1): a ring buffer of
 * decoded SCI observations over the sensing window, updated on reception
 * (event-driven, architecture principle 4 - never polled per slot), plus the
 * log of unmonitored slots (own transmissions, half-duplex).
 *
 * Plain C++ class (no cModule/omnetpp dependency, D13): "now" is always
 * passed in as a slot index, pruning is lazy on insert.
 */
class SlSensingDatabase
{
    int sensingWindowSlots_;               // T0 in slots
    std::deque<SlSensingEntry> entries_;   // ordered by non-decreasing slot
    std::deque<SlotIndex> unmonitoredSlots_;

  public:
    explicit SlSensingDatabase(int sensingWindowSlots = 1600) : sensingWindowSlots_(sensingWindowSlots) {}

    int getSensingWindowSlots() const { return sensingWindowSlots_; }

    /// record a decoded SCI; lazily prunes everything older than the sensing
    /// window relative to the entry's slot
    void recordSci(const SlSensingEntry& entry);

    /// record an own-TX (half-duplex) slot that could not be monitored
    void recordUnmonitoredSlot(SlotIndex slot);

    /// drop all records before the given slot
    void pruneBefore(SlotIndex slot);

    const std::deque<SlSensingEntry>& getEntries() const { return entries_; }
    const std::deque<SlotIndex>& getUnmonitoredSlots() const { return unmonitoredSlots_; }
    size_t size() const { return entries_.size(); }
};

} // namespace simu5g

#endif
