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
simple CompareChannelModel extends LteChannelModel
{
    parameters:
        @class("CompareChannelModel");

        // Paths to the two inner channel-model submodules (siblings of this one).
        string referenceModule;
        string candidateModule;

        // Whose result is returned to the simulation: "reference" (default, keeps
        // fingerprints stable) or "candidate".
        string primary @enum(reference,candidate) = default("reference");

        // SINR statistics emitted onto the channel model by other modules
        // (declared here because we extend LteChannelModel, not the realistic model)
        @signal[rcvdSinrDl];
        @statistic[rcvdSinrDl](title="SINR measured at packet reception, DL"; unit="dB"; source="rcvdSinrDl"; record=mean,vector);
        @signal[rcvdSinrUl];
        @statistic[rcvdSinrUl](title="SINR measured at packet reception, UL"; unit="dB"; source="rcvdSinrUl"; record=mean,vector);
        @signal[rcvdSinrD2D];
        @statistic[rcvdSinrD2D](title="SINR measured at packet reception, D2D"; unit="dB"; source="rcvdSinrD2D"; record=mean,vector);
        @signal[measuredSinrDl];
        @statistic[measuredSinrDl](title="SINR measured at feedback computation, DL"; unit="dB"; source="measuredSinrDl"; record=mean,vector);
        @signal[measuredSinrUl];
        @statistic[measuredSinrUl](title="SINR measured at feedback computation, UL"; unit="dB"; source="measuredSinrUl"; record=mean,vector);

        // per-call deltas (candidate - reference)
        @signal[attenuationDelta];
        @statistic[attenuationDelta](title="attenuation delta, candidate-reference"; unit="dB"; record=vector,stats);
        @signal[rsrpDelta];
        @statistic[rsrpDelta](title="RSRP delta (mean over bands), candidate-reference"; unit="dB"; record=vector,stats);
        @signal[sinrDelta];
        @statistic[sinrDelta](title="SINR delta (mean over bands), candidate-reference"; unit="dB"; record=vector,stats);
}
