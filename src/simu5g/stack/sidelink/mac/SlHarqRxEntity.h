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

#ifndef _SIDELINK_SLHARQRXENTITY_H_
#define _SIDELINK_SLHARQRXENTITY_H_

#include <map>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * Sidelink RX HARQ bookkeeping (design decision D2, WP-F): per-source,
 * per-process state used at slot-end decoding for
 *  - duplicate-delivery suppression by (source, process, NDI): once a TB is
 *    decoded, further blind copies are dropped;
 *  - the soft-combining model: the receiver-side attempt counter feeds the
 *    harqReduction-style effective-BLER scaling of the Uu error model.
 *
 * Plain C++ class (D13); owned by NrSlPhyUe since decoding (where combining
 * and dedup belong) happens at the PHY in this pipeline.
 */
class SlHarqRxEntity
{
    struct ProcessState {
        bool ndi = false;
        bool valid = false;      // any reception seen on this (src, proc) yet
        int attempts = 0;        // receptions seen for the current NDI
        bool delivered = false;  // TB of the current NDI already delivered up
    };

    std::map<std::pair<MacNodeId, int>, ProcessState> processes_;

  public:
    /// register a reception attempt; returns the 1-based combined-attempt
    /// count for the current TB (resets when NDI toggles)
    int onReception(MacNodeId src, int procId, bool ndi);

    /// whether the current TB of (src, proc) was already delivered upward
    bool isDelivered(MacNodeId src, int procId, bool ndi) const;

    void markDelivered(MacNodeId src, int procId, bool ndi);
};

} // namespace simu5g

#endif
