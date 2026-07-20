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

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

import simu5g.common.LteCommonEnum;
import simu5g.common.LteCommon;
import simu5g.stack.mac.packet.LteSchedulingGrant;

namespace simu5g;

//
// Configured-grant type 2 action carried by a mode-1 grant (D28/D30):
// a DCI-3_0-equivalent that activates or releases a dormant CG instead of
// carrying a dynamic allocation.
//
enum SlCgAction
{
    SL_CG_NONE = 0;
    SL_CG_ACTIVATE = 1;
    SL_CG_RELEASE = 2;
}

//
// Sidelink scheduling grant (design decision D28, SL-3): the DCI
// 3_0-equivalent issued by the gNB's SlEnbScheduler for mode-1 UEs. Rides
// the stock Uu GRANTPKT path (gNB sendLowerPackets -> UE Uu PHY ->
// NrMacUeSl::macHandleGrant, which routes it to the SL MAC without touching
// base Uu grant state, G22). The direction field is set to SL; the carrier
// tag is the gNB's Uu carrier (the DCI rides the Uu DL) while the sl*
// fields reference the SL pool's slot/subchannel grid.
//
// A dynamic grant is a finite occasion train: numOccasions = 1 initial +
// up to 2 preallocated retransmission occasions (DCI 3_0's "up to 2
// additional resources", D30), spaced occasionGapSlots apart. Configured
// grants (D30, WP-P) are periodic: slPeriodSlots > 0, numOccasions = 0;
// type 2 CGs are activated/released via cgAction.
//
class SlSchedulingGrant extends LteSchedulingGrant
{
    chunkLength = inet::B(1);
    int64_t slFirstSlot = -1;       // first TX occasion (absolute SL slot index)
    int slPeriodSlots = 0;          // CG period in SL slots (0 = dynamic/one-shot)
    int numOccasions = 1;           // finite-train length (0 = unbounded CG train)
    int occasionGapSlots = 0;       // spacing of the finite train's occasions
    int slFirstSubchannel = 0;
    int slNumSubchannels = 1;
    unsigned int slMcs = 0;         // PSSCH MCS the UE shall obey (gNB-configured, D29)
    int cgIndex = -1;               // configured-grant index (-1 = dynamic grant)
    SlCgAction cgAction = SL_CG_NONE;
}
