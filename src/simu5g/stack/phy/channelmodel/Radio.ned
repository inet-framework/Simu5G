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
// The full PHY link model: everything between a transmitted air frame and the
// decision on whether it was received. The channel impairments are modeled
// statistically -- drawn from the distributions of a 3GPP propagation study,
// rather than computed from the geometry of an actual environment -- which is
// what sets this model apart from ~IdealRadio, where there are no
// impairments at all, only a configured packet error rate.
//
// It covers:
// - path loss per deployment scenario, LOS/NLOS state and log-normal shadowing,
//   with the formulas of the 3GPP propagation study configured on the medium's
//   matching carrier leg (TR 36.814, TR 36.873 or TR 38.901);
// - multipath fading, Jakes or Rayleigh;
// - the antenna pattern attenuation and the link budget (antenna gains, cable
//   loss, noise figures, thermal noise);
// - interference from other cells: downlink, uplink, external cells and
//   background cells;
// - the assembly of all of the above into a per-band SINR, and the mapping of
//   that SINR onto a block error probability that decides reception.
//
// This endpoint declares only the facts that are genuinely per node: its
// antenna properties and noise figures, its outdoor-to-indoor geometry, and
// its statistics toggle. The environment -- the propagation study, deployment
// geometry, shadowing/fading/LOS model and the interference toggles -- is
// stated once for the whole network, on ~RadioMedium and its carrierLeg
// submodules.
//
simple Radio extends RadioBase
{
    parameters:
        @class("Radio");

        string radioMediumModule = default("radioMedium"); // module path of the medium this endpoint registers with

        // This radio's own antenna height: a per-node fact. Defaulted here
        // to the UE height; the eNB/gNB-side NICs override it to 25m.
        double height @unit(m) = default(1.5m);
        // determines if the UE is inside a building
        bool insideBuilding = default(false);

        // Antenna Gain of UE -->
        double antennaGainUe @unit(dBi) = default(0dBi);
        // Antenna Gain of eNodeB -->
        double antennGainEnB @unit(dBi) = default(18dBi);
        // Ue noise figure -->
        double ueNoiseFigure @unit(dBm) = default(7dBm);
        // eNodeB noise figure -->
        double bsNoiseFigure @unit(dBm) = default(5dBm);
        // Cable Loss -->
        double cableLoss @unit(dB) = default(2dB);

        // collection of SINR statistics can be disabled because it might be quite time-consuming
        bool collectSinrStatistics = default(true);

        // statistics
        @signal[rcvdSinrDl];
        @statistic[rcvdSinrDl](title="SINR measured at packet reception, DL"; unit="dB"; source="rcvdSinrDl"; record=mean,vector);
        @signal[rcvdSinrUl];
        @statistic[rcvdSinrUl](title="SINR measured at packet reception, UL"; unit="dB"; source="rcvdSinrUl"; record=mean,vector);

        @signal[measuredSinrDl];
        @statistic[measuredSinrDl](title="SINR measured at feedback computation, DL"; unit="dB"; source="measuredSinrDl"; record=mean,vector);
        @signal[measuredSinrUl];
        @statistic[measuredSinrUl](title="SINR measured at feedback computation, UL"; unit="dB"; source="measuredSinrUl"; record=mean,vector);
}

