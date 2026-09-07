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
// One carrier leg's deployment geometry and propagation study: the routing
// keys a registering radio is matched against (componentCarrierModule, leg),
// the scenario/geometry parameters the two path-loss strategies below are
// initialized from, and the strategies themselves. A ~RadioMedium normally
// declares a single, match-anything entry; a
// network whose radios genuinely run two different studies on two different
// legs (e.g. LTE + NR dual connectivity) declares two.
//
// No nodebHeight/ueHeight: antenna heights are per-node facts on the radio
// endpoint, threaded per call in a LinkContext, not stated here.
//
module CarrierLegPhysics
{
    parameters:
        @display("i=block/classifier2");

        // Which registering radios this entry configures. "" matches any
        // carrier frequency; a non-empty path names a specific
        // ComponentCarrier module and restricts the match to radios on that
        // carrier's frequency.
        string componentCarrierModule = default("");
        // "LTE"/"NR" restricts the match to that leg; "any" matches both.
        string leg @enum("LTE","NR","any") = default("any");

        // Deployment scenario and geometry, fed to both path-loss strategies
        // below.
        string scenario @enum(INDOOR_HOTSPOT,URBAN_MICROCELL,URBAN_MACROCELL,
                              RURAL_MACROCELL,SUBURBAN_MACROCELL,UNKNOWN_SCENARIO)
                              = default("URBAN_MACROCELL");
        double buildingHeight @unit(m) = default(20m);
        double streetWidth @unit(m) = default(20m);
        bool tolerateMaxDistViolation = default(false);
    submodules:
        // Defaults to the historic endpoint study (TR 36.814): every
        // network may still state its propagation study explicitly, but is
        // no longer required to.
        pathLoss: <default("Tr36814PathLoss")> like IPathLossModel {
            @display("p=100,60");
        }
        // Always TR 36.814, regardless of pathLoss's study: the ext-cell and
        // background-cell interference paths use those formulas
        // unconditionally.
        extCellPathLoss: <default("Tr36814PathLoss")> like IPathLossModel {
            @display("p=100,140");
        }
}
