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
module NrSlMacUe extends LteMacBase
{
    parameters:
        @class(NrSlMacUe);
        @display("i=block/mac");

        string slRrcModule = default("^.slRrc");

        // resource allocation: "mode2" = TS 38.321 5.22 sensing-based
        // selection; "random" = same window, no sensing (baseline);
        // "static" = fixed preconfigured grant per UE (staticGrant* below)
        string resourceAllocationMode @enum(mode2,random,static) = default("mode2");

        // --- selected-grant shape (all modes) ---
        int grantNumSubchannels = default(1);         // L_subCH of selected resources
        int grantMcs = default(6);                    // PSSCH MCS (used as the CQI index of the BLER lookup)
        int tbSize @unit(B) = default(1000B);         // bytes deliverable per TX opportunity (MCS/TBS stub)

        // --- mode2 only ---
        double probResourceKeep = default(0);         // P_keep on reselection-counter expiry [0..0.8]

        // --- static mode only ---
        int staticGrantSlotOffset = default(0);       // absolute slot offset of this UE's grant train
        int staticGrantFirstSubchannel = default(0);
}
