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

#ifndef _SIDELINK_SLPRECONFIG_H_
#define _SIDELINK_SLPRECONFIG_H_

#include <string>
#include <vector>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace omnetpp { class cValueMap; }

namespace simu5g {

/**
 * One entry of the slrbConfig array (design decision D12): the static SLRB
 * configuration used until PC5-RRC negotiation exists (SL-2). Format mirrors
 * SDAP's drbConfig/DrbTable JSON precedent.
 */
struct SlrbConfigEntry
{
    SlL2Id dstL2Id = SL_L2ID_NONE;
    SlCastType castType = SL_BROADCAST;
    DrbId drbId = DRBID_NONE;
    LteRlcType rlcType = UM;
    std::string destAddress;    // optional: IPv4 multicast address mapped to dstL2Id
};

/**
 * Sidelink preconfiguration (TS 38.331 SL-PreconfigurationNR analog, heavily
 * abstracted): resource pool geometry, mode-2 selection parameters and the
 * static SLRB configuration. Parsed from a JSON-style NED object parameter;
 * unit-parsable without a network.
 */
class SlPreconfig
{
  public:
    // --- resource pool ---
    double carrierFrequencyGHz = 5.9;      // SL carrier (ITS band default)
    unsigned int numerologyIndex = 0;      // slot duration = 1ms / 2^numerologyIndex
    int subchannelSize = 10;               // PRBs per subchannel
    int numSubchannels = 5;
    std::string slotBitmap = "all";        // pool slot bitmap; "all" = every slot belongs to the pool

    // --- mode-2 selection parameters (consumed from WP-E on) ---
    int t0Ms = 1000;                       // sensing window length [ms] (TS 38.214: 1000 or 100)
    int t1 = 2;                            // selection window start offset [slots]
    int t2 = 20;                           // selection window end offset [slots]
    double rsrpThresholdDbm = -128;        // initial exclusion threshold
    std::vector<int> reservationPeriodsMs = { 100 };  // allowed resource reservation periods
    int blindRetx = 0;                     // blind retransmissions per TB (WP-F)
    int psfchPeriod = 0;                   // 0 = no PSFCH (SL-1); placeholder for SL-2

    // --- static SLRB configuration (D12) ---
    std::vector<SlrbConfigEntry> slrbConfig;

    void loadFromJson(const omnetpp::cValueMap *map);

    /// slot duration on this pool's numerology grid
    omnetpp::simtime_t getSlotDuration() const { return omnetpp::SimTime(1, omnetpp::SIMTIME_MS) / (1 << numerologyIndex); }

    const SlrbConfigEntry *findSlrbForDstL2Id(SlL2Id dstL2Id) const;
    const SlrbConfigEntry *findSlrbForDestAddress(const std::string& addr) const;
};

} // namespace simu5g

#endif
