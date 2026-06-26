//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel;

//
// CompareChannelModel
//
// Validation decorator (Plan A §7). It is itself an ILteChannelModel and is
// installed as the PHY's channel model, but it forwards every call to two sibling
// channel-model submodules referenced by parameter path - a 'reference' (the
// built-in model, e.g. NrChannelModel_3GPP38_901) and a 'candidate'
// (SionnaChannelModel) - records their per-call differences, and returns the
// 'primary' model's result to the simulation.
//
// RNG neutrality: the built-in model draws random numbers (shadowing, fading) and
// the reception success draw consumes RNG; the Sionna model draws nothing. With
// primary = reference (the default), only the primary's draws happen, so the run
// behaves exactly like a normal built-in run, with Sionna deltas logged on top.
//
// Placement: use it on the UE side (DL validation), as in the SionnaCompare example.
// Placing it (and its two inner models) at an eNB makes all three register the same
// carrier on the cell's CellInfo, which CellInfo::registerCarrier rejects as a
// duplicate. (On a UE, CellInfo is absent, so no carrier is registered.)
//
simple CompareChannelModel extends LteRealisticChannelModel
{
    parameters:
        @class("CompareChannelModel");

        // Paths to the two inner channel-model submodules (siblings of this one).
        string referenceModule;
        string candidateModule;

        // Whose result is returned to the simulation: "reference" (default, keeps
        // fingerprints stable) or "candidate".
        string primary @enum(reference,candidate) = default("reference");

        // The rcvdSinr*/measuredSinr* statistics (emitted onto this module by other
        // modules) are inherited from LteRealisticChannelModel.

        // per-call deltas (candidate - reference)
        @signal[attenuationDelta];
        @statistic[attenuationDelta](title="attenuation delta, candidate-reference"; unit="dB"; record=vector,stats);
        @signal[rsrpDelta];
        @statistic[rsrpDelta](title="RSRP delta (mean over bands), candidate-reference"; unit="dB"; record=vector,stats);
        @signal[sinrDelta];
        @statistic[sinrDelta](title="SINR delta (mean over bands), candidate-reference"; unit="dB"; record=vector,stats);
}
