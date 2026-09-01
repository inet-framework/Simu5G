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
// The central, network-level module that models the shared physical radio
// medium of a cellular network. It is the single owner of the physical facts
// and channel effects -- path loss, shadowing, fading and interference -- of
// every radio link, shared by every carrier and every radio endpoint that
// registers with it.
//
module RadioMedium like IRadioMedium
{
    parameters:
        @class(RadioMedium);
        @display("i=misc/sun");

        // The environment, stated once for the whole network: every radio
        // registered with this medium shares one value of each, so stating
        // it once replaces stating it identically on every endpoint.
        bool shadowing = default(true);
        double correlationDistance @unit(m) = default(50m);
        bool dynamicLos = default(false);
        bool fixedLos = default(false);
        bool enableExtCellLos = default(true);
        bool fading = default(true);
        string fadingType @enum(RAYLEIGH,JAKES) = default("JAKES");
        int numFadingPaths = default(6);
        double delayRms @unit(s) = default(363ns);
        double thermalNoise @unit(dBm) = default(-104.5dBm);
        bool useTorus = default(false);
        double targetBler = default(0.01);
        double harqReduction = default(0.2);
        bool bgCellInterference = default(true);
        bool extCellInterference = default(true);
        bool downlinkInterference = default(false);
        bool uplinkInterference = default(false);
        bool d2dInterference = default(true); // only D2D-capable radios care

        // Number of carrier legs this medium's radios run distinct
        // propagation studies on. 1 (the default) covers every in-tree
        // network but the few that genuinely run two studies at once, e.g.
        // LTE + NR dual connectivity -- see carrierLeg below.
        int numCarrierLegs = default(1);
    submodules:
        interference: <default("CellularInterferenceModel")> like IInterferenceModel {
            @display("p=100,100");
        }
        // MUST stay declared after interference, and radioMedium must stay
        // the last network-level submodule in every network: OMNeT++
        // creates a compound module's direct
        // children before building any sibling's subtree, so this is what
        // guarantees these submodules get the highest module ids in the
        // network and shift no other module's id.
        carrierLeg[numCarrierLegs]: CarrierLegPhysics {
            @display("p=100,180");
        }
}
