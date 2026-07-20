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

package simu5g.stack.sidelink.phy;

import simu5g.stack.phy.NrPhyUe;

//
// SL-aware NR Uu PHY for the UE (D32, SL-3): the Uu leg of the opt-in
// Uu/SL half-duplex arbiter. With sharedUuSlRadio=true it records every
// UL airframe (data, RAC, BSR PDUs, CQI feedback) in the UE's shared
// radio state and drops arriving Uu DATA frames that overlap an SL
// transmission (Uu control stays lossless, G6). TX-TX conflicts are
// counted (slUuTxConflicts), not suppressed. With the default
// sharedUuSlRadio=false it behaves exactly like ~NrPhyUe.
//
// Selected by the hasSidelink conditional default of the nrPhy slot in
// ~NrNicUe.
//
module NrPhyUeSl extends NrPhyUe
{
    parameters:
        @class("NrPhyUeSl");

        // model the single-radio Uu/SL constraint (D32); both PHY legs of
        // the UE must agree on this setting
        bool sharedUuSlRadio = default(false);

        // recorders are opt-in ('?') so arbiter-off runs stay result-identical;
        // enable with **.result-recording-modes = all on these statistics
        @signal[uuHalfDuplexSlDrops](type=long);
        @statistic[uuHalfDuplexSlDrops](title="Uu data frames lost to SL TX (half-duplex)"; record=count?,vector?);
        @signal[slUuTxConflicts](type=long);
        @statistic[slUuTxConflicts](title="simultaneous Uu+SL TX conflicts (counted, not suppressed)"; record=count?,vector?);
}
