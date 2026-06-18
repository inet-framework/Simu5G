//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel.sionna;


//
// Loads the offline Sionna artifact once and asserts the parameter contract against
// the live scenario, failing loud on mismatch. Placed once at NIC or network scope;
// ~SionnaChannelModel instances acquire the loaded table from it.
//
simple SionnaManager
{
    parameters:
        @class("SionnaManager");
        string artifactManifest = default("sionna_artifact/manifest.json");
        // Live-scenario contract values asserted against the manifest at init (CAL-02).
        // Any mismatch aborts the run with cRuntimeError (no silent fallback).
        double carrierFrequencyHz;
        double subcarrierSpacingHz;
        int numBands;
        // Optional: module path to the live ComponentCarrier submodule whose parameters
        // the PHY actually uses (e.g. "SionnaSingleLink.carrierAggregation.componentCarrier[0]").
        // When non-empty, SionnaManager reads carrierFrequency/numerologyIndex/numBands from
        // that module and cross-checks them against the shadow params above — a hand-copy
        // divergence aborts the run loud (CR-01 interim hardening).
        // When empty (default) the cross-check is skipped and existing behaviour is unchanged.
        // TODO(phase-2): replace shadow params entirely with a direct bind to componentCarrier.
        string componentCarrierModule = default("");
}
