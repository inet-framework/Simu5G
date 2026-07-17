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

#ifndef _SIDELINK_SLCOMMON_H_
#define _SIDELINK_SLCOMMON_H_

#include <cstdint>
#include <string>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

//
// Fundamental NR sidelink (PC5) types. See TS 38.300 §16.9.
//

/// 24-bit sidelink Layer-2 ID (source or destination), config/model-level
/// identifier chosen by the upper layers (V2X application). Internally each
/// L2 ID in use maps to a MacNodeId ("L2Pid") via SlBinder: a UE's source
/// L2 ID maps to the UE's own (NR) MAC node id, broadcast/groupcast
/// destination L2 IDs map to allocated pseudo node ids.
typedef uint32_t SlL2Id;
constexpr SlL2Id SL_L2ID_NONE = 0;

/// Conventional "all UEs" broadcast destination L2 ID used by examples/tests
constexpr SlL2Id SL_L2ID_BROADCAST = 0xFFFFFF;

/// Sidelink cast type (TS 38.300 §16.9.3), selected per destination L2 ID
enum SlCastType {
    SL_BROADCAST = 0,
    SL_GROUPCAST = 1,
    SL_UNICAST = 2,
};

const std::string slCastTypeToA(SlCastType c);
SlCastType aToSlCastType(const std::string& s);

/// PSFCH HARQ feedback mode of an SLRB (design decision D19/D24). Unicast
/// SLRBs use ACK/NACK implicitly whenever the pool has PSFCH configured;
/// groupcast SLRBs pick option 1 (distance-gated NACK-only, needs mcr) or
/// option 2 (per-member ACK/NACK) explicitly; broadcast stays on blind retx.
enum SlPsfchMode {
    SL_PSFCH_OFF = 0,
    SL_PSFCH_NACK_ONLY = 1,   // groupcast option 1
    SL_PSFCH_ACK_NACK = 2,    // groupcast option 2 / unicast
};

const std::string slPsfchModeToA(SlPsfchMode m);
SlPsfchMode aToSlPsfchMode(const std::string& s);

/// Absolute slot index on the numerology grid of the SL carrier
/// (slot 0 starts at simtime 0; slot duration = 1ms / 2^numerologyIndex)
typedef int64_t SlotIndex;
constexpr SlotIndex SLOTINDEX_NONE = -1;

/// Pseudo node-id ("L2Pid") allocation range for sidelink broadcast/groupcast
/// destinations. Allocated downward from SL_GROUP_PID_MAX so it stays disjoint
/// from the Uu multicast destination-id allocator in Binder, which counts
/// upward from MULTICAST_DEST_MIN_ID (32768).
constexpr unsigned short SL_GROUP_PID_MAX = 65535;

/// DRB-id partitioning of the sidelink bearer space (D17/D23): static
/// slrbConfig entries must stay below SL_UNICAST_DRB_BASE; per-link unicast
/// SLRBs are allocated dynamically from [SL_UNICAST_DRB_BASE, SL_SRB_DRB_ID);
/// SL_SRB_DRB_ID is the reserved TM SL-SRB of a unicast link (PC5-RRC).
/// Keeping the ranges disjoint preserves the D3 keying invariant: at a
/// receiver, broadcast RX chains and unicast RX chains from the same sender
/// are both keyed DrbKey(senderId, drb) and must never collide.
constexpr unsigned short SL_UNICAST_DRB_BASE = 32;
constexpr unsigned short SL_SRB_DRB_ID = 63;

/**
 * Mode-2 sidelink grant (design decision D8): the outcome of resource
 * (re)selection, owned by the SL MAC. It is UE-internal state and is never
 * transmitted (Mode 2 grants are self-assigned), hence a plain struct and
 * not a .msg class. Mode 1 (gNB-scheduled, phase SL-3) will introduce a
 * packet-borne grant separately.
 */
struct SlGrant
{
    SlotIndex firstSlot = SLOTINDEX_NONE;  // first TX slot (absolute slot index)
    int periodSlots = 0;                   // resource reservation period in slots (0 = one-shot)
    int firstSubchannel = 0;               // first subchannel of the selected resource
    int numSubchannels = 1;                // L_subCH: width of the selected resource
    unsigned int mcs = 0;                  // modulation and coding scheme for PSSCH
    int reselectionCounter = 0;            // remaining periods until reselection (0 = static/preconfigured grant)
    int blindRetx = 0;                     // number of blind retransmissions per TB

    bool isValid() const { return firstSlot != SLOTINDEX_NONE; }
};

} // namespace simu5g

#endif
