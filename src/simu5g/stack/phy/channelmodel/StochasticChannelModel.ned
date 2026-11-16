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

package simu5g.stack.phy.channelmodel;

//
// The radio medium's channel model of one carrier and RAT (see
// ~CellularRadioMedium): everything between a transmitted air frame and the
// SINR it arrives with, which the receiving radio's error model and receiver
// turn into the decision on whether it was received. The channel impairments
// are modeled statistically -- drawn from the distributions of a 3GPP
// propagation study, rather than computed from the geometry of an actual
// environment -- which is what sets this model apart from ~IdealChannelModel,
// where there are no impairments at all (and, with a
// ~ConfiguredRateErrorModel at the receivers, only a configured packet error
// rate).
//
// It covers:
// - path loss per deployment scenario, LOS/NLOS state and log-normal shadowing,
//   with the formulas of the 3GPP propagation study selected by pathLossType
//   (TR 36.814, TR 36.873 or TR 38.901);
// - multipath fading, Jakes or Rayleigh;
// - the antenna pattern attenuation and the link budget (the radios' antenna
//   gains, cable loss and noise figure; thermal noise);
// - interference from other cells: downlink, uplink, external cells and
//   background cells;
// - the assembly of all of the above into a per-band SINR, at a reception or
//   in a what-if (a CQI, a cell-selection probe).
//
// Only the path loss, LOS probability, shadowing and angular attenuation come
// from the selected propagation study; everything else is the same whichever
// study is selected. The ~Tr36873ChannelModel and ~Tr38901ChannelModel presets
// differ from this type only in their pathLossType default.
//
// A link is evaluated in the deployment of its base station end: the
// scenario, the average building height and the street width are parameters
// of the base station's radio (~CellularRadio, or the ~CellularPhantomRadio of
// a background or external cell), so the cells of a carrier may differ in
// them. The heights the formulas take are those of the link's two antennas
// above the ground (z = 0): an antenna is at its node's z plus its
// antenna.mobility.offsetZ (see ~CellularAntenna). A link whose heights are
// outside what the study gives for the scenario is an error, unless
// tolerateHeightViolation is set.
//
// A link's LOS state, with its effective environment height, and its shadowing
// belong to the two ends of the channel, not to the radios at them: they are
// the same on every carrier, for both radios of a dual-stack UE, and for
// every cell that gives the same site name (the site parameter of a base
// station's radio: the sectors of a site). The fast fading is drawn per pair
// of radios and per carrier. The first channel model that evaluates a pair of
// ends draws their state; a model of another carrier, RAT or study takes it.
//
// A link between two UEs (D2D) has no base station, and none of the three
// studies has such a link. The model evaluates it with the scenario of the
// receiving UE's serving cell, taking the base station height the studies give for that scenario (10 m in
// an urban microcell, 25 m in an urban macrocell, 35 m in a suburban or rural
// macrocell, 3 m in an indoor hotspot) and the lower of the two UEs' antenna
// heights. This is a convention of the model, not a specification's.
//
simple StochasticChannelModel extends ChannelModelBase
{
    parameters:
        @class("StochasticChannelModel");

        string pathLossType @enum("Tr36814","Tr36873","Tr38901") = default("Tr36814"); // Which 3GPP propagation study supplies the path loss formulas

        // Enable/disable shadowing
        bool shadowing = default(true);

        // Removed: the scenario is the base station's (the scenario parameter of its radio); setting it is an error
        string scenario = default("");

        // Removed: the base station's height is its antenna's (antenna.mobility.offsetZ above the node); setting it is an error
        double nodebHeight @unit(m) = default(nan m);
        // Removed: the average building height is the base station's (the buildingHeight parameter of its radio); setting it is an error
        double buildingHeight @unit(m) = default(nan m);
        // Enable the high-loss building penetration model (table 7.4.3-2 in TR 38.901) for a UE inside a building (see ~CellularRadio); only used with pathLossType="Tr38901"
        bool useBuildingPenetrationHighLossModel = default(false);

        // Removed: the average street width is the base station's (the streetWidth parameter of its radio); setting it is an error
        double streetWidth @unit(m) = default(nan m);
        // Removed: the UE's height is its antenna's (antenna.mobility.offsetZ above the node); setting it is an error
        double ueHeight @unit(m) = default(nan m);
        bool tolerateMaxDistViolation = default(false);
        // Evaluate a link although its antenna heights are outside the range the study gives for the scenario
        bool tolerateHeightViolation = default(false);

        // How far a link's ends must move before its LOS state and its shadowing
        // are drawn again: the shadowing is redrawn, correlated with the previous
        // sample, when either end has moved farther than this since that sample
        double correlationDistance @unit(m) = default(50m);

        // Target BLER used to compute feedback
        double targetBler = default(0.01);

        // Thermal noise for 10 MHz of bandwidth
        double thermalNoise @unit(dBm) = default(-104.5dBm);

        // Whether LOS/NLOS is drawn per link from the scenario's LOS probability; when false, it is fixed to fixedLos
        bool dynamicLos = default(false);
        // LOS state of every link when dynamicLos is false: true = LOS, false = NLOS
        bool fixedLos = default(false);
        // Enable/disable fading
        bool fading = default(true);
        // Fading type (JAKES or RAYLEIGH)
        string fadingType @enum(RAYLEIGH,JAKES) = default("JAKES");
        // If Jakes fading, this parameter specifies the number of paths (tap channel)
        int numFadingPaths = default(6);

        double delayRms @unit(s) = default(363ns);

        // Whether interference from background cells is included in the SINR, in both UL and DL
        bool bgCellInterference = default(true);
        // Whether interference from external cells is included in the DL SINR
        bool extCellInterference = default(true);
        // Whether interference from other base stations is included in the DL SINR
        bool downlinkInterference = default(false);
        // Whether interference from the UEs of other cells is included in the UL SINR
        bool uplinkInterference = default(false);
        // Whether interference from other D2D transmissions is included in the D2D SINR
        bool d2dInterference = default(true);

        // Whether a phantom link (an external or background cell's, or a background UE's) takes its own LOS state; when false, it is NLOS
        bool enableExtCellLos = default(true);

        // Collection of SINR statistics can be disabled because it might be quite time-consuming.
        // The statistics are recorded by the UE's receiver (see ~CellularReceiver).
        bool collectSinrStatistics = default(true);
}

