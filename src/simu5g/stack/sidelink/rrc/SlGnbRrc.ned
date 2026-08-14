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

package simu5g.stack.sidelink.rrc;

//
// gNB-side sidelink control plane (D25, SL-3): owns the cell's sidelink
// resource pool configuration and the registry of sidelink UEs served by
// the cell. Attached UEs whose ~SlRrc has poolSource="servingCell" resolve
// their pool from here ("SIB12-equivalent"). The provisioning is genie:
// no over-the-air system information transport is modeled - the UE copies
// the pool section at init time (documented simplification).
//
// Instantiated inside ~NrNicEnbSl (selected by the gNodeB's hasSidelink
// parameter). From WP-O on it also owns the Mode-1 resource allocator
// (SlEnbScheduler).
//
simple SlGnbRrc
{
    parameters:
        @display("i=block/control");

        // The cell's SL resource pool: the *pool section* of the ~SlRrc
        // preconfig JSON shape (carrier, numerology, subchannel geometry,
        // mode-2 selection params, PSFCH fields). Bearer-side sections
        // (slrbConfig, unicastSlrbDefaults, cbrConfig, ...) are rejected:
        // they stay UE-local. Example:
        //   { carrierFrequency: 5.9GHz, numerologyIndex: 1,
        //     subchannelSize: 10, numSubchannels: 5,
        //     t1: 2, t2: 20, rsrpThresholdDbm: -128,
        //     reservationPeriodsMs: [20] }
        object slPoolConfig = default({});

        // --- mode-1 dynamic-grant allocation (D28/D29/D30) ---

        // PSSCH MCS carried in every grant (no SL CSI loop is modeled - the
        // gNB MCS is configured, the UE obeys it)
        int grantMcs = default(6);

        // per-slot symbols lost to AGC + guard + PSCCH + DMRS in the TBS
        // math; must match the UEs' slMac.overheadSymbols
        int overheadSymbols = default(5);

        // minimum gap between grant issue and its first occasion [SL slots]
        // (models the UE's DCI decode + PSSCH preparation time)
        int ueProcessingSlots = default(3);

        // occasions per dynamic grant: 1 initial + preallocated blind-retx
        // occasions (DCI 3_0 allows up to 2 additional resources)
        int grantNumOccasions = default(3);

        // spacing of a dynamic grant's occasions [SL slots]. For PSFCH-driven
        // retransmissions to be usable the gap must exceed the pool's PSFCH
        // feedback delay (psfchPeriod + psfchMinGap); not enforced.
        int occasionGapSlots = default(4);

        // give up allocating when no free train exists within this window
        int schedulingHorizonSlots = default(256);
}
