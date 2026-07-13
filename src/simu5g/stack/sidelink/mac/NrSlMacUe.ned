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

package simu5g.stack.sidelink.mac;

import simu5g.stack.mac.LteMacBase;

//
// Sidelink MAC of a UE (design decision D1): event-driven (no per-slot
// ticker), TX-slot self-events on the absolute slot grid of the SL carrier.
//
// Phase SL-1 / WP-C: fixed preconfigured periodic grant per UE; the mode-2
// sensing-based resource selector (TS 38.321 5.22) replaces it in WP-E.
//
module NrSlMacUe extends LteMacBase
{
    parameters:
        @class(NrSlMacUe);
        @display("i=block/mac");

        string slRrcModule = default("^.slRrc");

        // --- static grant (WP-C stub, replaced by SlMode2Selector in WP-E) ---
        int staticGrantSlotOffset = default(0);       // absolute slot offset of this UE's grant train
        int staticGrantFirstSubchannel = default(0);
        int staticGrantNumSubchannels = default(1);
        int staticGrantMcs = default(6);
        int tbSize @unit(B) = default(1000B);         // bytes deliverable per TX opportunity (MCS/TBS stub)
}
