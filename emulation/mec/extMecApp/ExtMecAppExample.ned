//
//                           Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.emulation.mec.extMecApp;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.corenetwork.smf.Smf;
import simu5g.nodes.NrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.nodes.MecHost;
import simu5g.mec.orchestrator.MecOrchestrator;
import simu5g.mec.ualcmp.Ualcmp;
import simu5g.stack.phy.channelmodel.RadioMedium;
import simu5g.world.radio.LteChannelControl;


network ExtMecAppExample
{
    parameters:
        // carrierLeg[0] is the real, traffic-carrying NR leg (what ini
        // Configs override to switch study); carrierLeg[1] is the vestigial
        // LTE leg of the NR UEs, always at the historic Tr36814 default,
        // never traffic-carrying, never overridden in this tree. A single
        // match-anything leg would force one study onto both.
        radioMedium.numCarrierLegs = default(2);
        radioMedium.carrierLeg[0].leg = default("NR");
        radioMedium.carrierLeg[0].pathLoss.typename = default("Tr38901PathLoss");
        radioMedium.carrierLeg[1].leg = default("LTE");

        double playgroundSizeX @unit(m); // x size of the area the nodes are in (in meters)
        double playgroundSizeY @unit(m); // y size of the area the nodes are in (in meters)
        double playgroundSizeZ @unit(m); // z size of the area the nodes are in (in meters)
        @display("bgb=805,500");

    submodules:
        routingRecorder: RoutingTableRecorder {
            @display("p=690,104;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=556,34");
        }
        channelControl: LteChannelControl {
            @display("p=690,24;is=s");
        }
        binder: Binder {
            @display("p=474,34;is=s");
        }
        smf: Smf {
            @display("p=474,94;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=368,34;is=s");
        }
        upf: Upf {
            @display("p=420,259");
        }
        iUpf: Upf {
            @display("p=277,259");
        }
        gNodeB1: gNodeB {
            @display("p=137,260;is=vl");
        }

        //# MEC modules
        mecHost1: MecHost {
            @display("p=277,158;is=l");
        }

        mecOrchestrator: MecOrchestrator {
            @display("p=520,158;is=l");
        }

        ualcmp: Ualcmp {
            @display("p=420,157;i=device/smallrouter");
        }

        ue: NrUe {
            @display("p=51,259");
        }
        radioMedium: RadioMedium {
            @display("p=368,117;is=s");
        }


    connections allowunconnected:

        gNodeB1.ppp <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> upf.pppg++;

	    //#	mec
        mecHost1.ppp++ <--> Eth10G <--> iUpf.pppg++;
        upf.dnPppg <--> Eth10G <--> ualcmp.ppp++;

        ualcmp.toMecOrchestrator --> mecOrchestrator.fromUALCMP;
        ualcmp.fromMecOrchestrator <-- mecOrchestrator.toUALCMP;
}
