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

#include <map>
#include <string>
#include <vector>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace omnetpp { class cValueMap; }

namespace simu5g {

/**
 * One entry of the slrbConfig / unicastSlrbDefaults arrays (design decisions
 * D12/D17, entry shape extended per G17). Format mirrors SDAP's
 * drbConfig/DrbTable JSON precedent.
 *
 * slrbConfig entries (static broadcast/groupcast SLRBs) carry an explicit
 * dstL2Id and drb; unicastSlrbDefaults entries are per-link templates -- their
 * DRB ids are allocated dynamically at link establishment (D17), so they
 * carry no dstL2Id/drb.
 */
struct SlrbConfigEntry
{
    SlL2Id dstL2Id = SL_L2ID_NONE;
    SlCastType castType = SL_BROADCAST;
    DrbId drbId = DRBID_NONE;
    LteRlcType rlcType = UM;
    std::string destAddress;    // optional: IPv4 multicast address mapped to dstL2Id
    int pfi = 0;                // PC5 QoS flow identifier served by this SLRB (SL-SDAP, WP-J)
    int pqi = 0;                // PC5 5QI of the flow (priority mapping, WP-J)
    bool isDefault = false;     // catches packets with no matching PFI (WP-J)
    double mcrMeters = 0;       // groupcast option-1 minimum communication range (WP-I; 0 = none)
    SlPsfchMode psfchMode = SL_PSFCH_OFF;  // groupcast HARQ feedback option (WP-I); unicast
                                           // SLRBs use ACK/NACK implicitly when the pool has PSFCH
};

/**
 * One level of the CBR-based congestion control table (design decision D22,
 * TS 38.214 8.1.6-style, abstracted): the level applies while the measured
 * CBR is <= cbrUpper (levels sorted ascending; the last level should have
 * cbrUpper 1.0).
 */
struct SlCbrLevel
{
    double cbrUpper = 1.0;          // level applies while CBR <= cbrUpper
    unsigned int maxMcs = 28;       // MCS cap at (re)selection
    int maxNumSubchannels = 1000;   // L_subCH cap at (re)selection
    double maxTxPowerDbm = 1000;    // TX power cap, applied at slPhy
    double crLimit = 1.0;           // own channel-occupancy-ratio limit (TX occasions defer above it)
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
    double txPercentage = 0.2;             // X of TS 38.214 8.1.4 (sl-TxPercentageList), as a ratio
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

    // --- PSFCH (D19; consumed from SL-2 WP-I on) ---
    int psfchPeriod = 0;                   // PSFCH slots are those with slot % period == 0; 0 = no PSFCH
    int psfchMinGap = 2;                   // min PSSCH-to-PSFCH gap [slots] (TS 38.213 MinTimeGapPSFCH analog)
    int psfchResources = 8;                // feedback resource indices per PSFCH slot (collision abstraction)

    // --- static SLRB configuration (D12) ---
    std::vector<SlrbConfigEntry> slrbConfig;

    // --- per-link SLRB templates for PC5 unicast (D17): one entry per PFI;
    //     DRB ids are allocated at link establishment. Default: one UM SLRB.
    std::vector<SlrbConfigEntry> unicastSlrbDefaults;

    // --- PQI priority overrides (WP-J; on top of the hardcoded TS 23.287
    //     subset, see slPqiToPriority) ---
    std::map<int, int> pqiPriorityOverrides;

    /// LCP priority of a PQI: preconfig override, or the standard subset
    int getPqiPriority(int pqi) const;

    // --- CBR congestion control levels (D22, WP-K); empty = off ---
    std::vector<SlCbrLevel> cbrConfig;

    /// the level applying to a measured CBR (nullptr when the table is empty)
    const SlCbrLevel *findCbrLevel(double cbr) const;

    SlPreconfig();

    void loadFromJson(const omnetpp::cValueMap *map);

  private:
    /// parse the shared (G17) entry-shape fields of an SLRB config entry
    static void loadSlrbEntryShape(const omnetpp::cValueMap *entry, SlrbConfigEntry& e);

  public:

    /// slot duration on this pool's numerology grid
    omnetpp::simtime_t getSlotDuration() const { return omnetpp::SimTime(1, omnetpp::SIMTIME_MS) / (1 << numerologyIndex); }

    const SlrbConfigEntry *findSlrbForDstL2Id(SlL2Id dstL2Id) const;
    const SlrbConfigEntry *findSlrbForDestAddress(const std::string& addr) const;
};

} // namespace simu5g

#endif
