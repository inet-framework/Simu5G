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
package simu5g.simulations.nr.networks;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.corenetwork.bearerConfigurator.BearerConfigurator;
import simu5g.nodes.NrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.PgwStandard;
import simu5g.nodes.BackgroundCell;
import simu5g.nodes.eNodeB;
import simu5g.stack.phy.channelmodel.RadioMedium;
import simu5g.world.radio.LteChannelControl;


// SingleCell_withSecondaryEnb
//
// The mirror image of ~SingleCell_withSecondaryGnb: here the gNodeB is the master
// node and the eNodeB is its secondary, so the UE is anchored on its NR stack and
// its LTE stack carries the secondary cell group. All data-plane traffic enters at
// the gNodeB.
//
// The core network is the LTE EPC, as in the EN-DC network. A real NR-anchored
// deployment pairs an NR master with a 5GC, which would make these bearers "5gc"
// bearers and put SDAP in the stack; that combination has never been exercised,
// so this network is deliberately kept EPC-flavored until the QoS-flow work lands.
//
network SingleCell_withSecondaryEnb
{
    parameters:
        // secondaryEnb (LTE, Tr36814) and masterGnb (NR, Tr38901) are
        // genuinely two active legs, not the vestigial-leg case a single
        // match-anything entry covers -- both carry traffic (dual
        // connectivity), so each needs its own study.
        radioMedium.numCarrierLegs = default(2);
        radioMedium.carrierLeg[0].leg = default("LTE");
        radioMedium.carrierLeg[1].leg = default("NR");
        radioMedium.carrierLeg[1].pathLoss.typename = default("Tr38901PathLoss");
        int numUe = default(1);
        int numBgCells = default(0);
        @display("i=block/network2;bgb=991,558;bgi=background/pisa");
    submodules:
        channelControl: LteChannelControl {
            @display("p=87,24;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=88,85;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125");
        }
        binder: Binder {
            @display("p=88,230;is=s");
        }
        bearerConfigurator: BearerConfigurator {
            @display("p=148,230;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=88,290;is=s");
        }
        server: StandardHost {
            @display("p=210,63;is=n;i=device/server");
        }
        router: Router {
            @display("p=361,62;i=device/smallrouter");
        }
        pgw: PgwStandard {
            @display("p=524,63;is=l");
        }
        masterGnb: gNodeB {
            @display("p=527,277;is=vl");
        }
        secondaryEnb: eNodeB {
            @display("p=726,277;is=vl");
        }
        bgCell[numBgCells]: BackgroundCell {
            @display("p=910,98;is=vl");
        }
        ue[numUe]: NrUe {
            @display("p=628,411");
        }
        radioMedium: RadioMedium {
            @display("p=88,373;is=s");
        }

    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> pgw.dnPppg;
        pgw.pppg++ <--> Eth10G <--> masterGnb.ppp;

        //# X2 connections
        masterGnb.x2++ <--> Eth10G <--> secondaryEnb.x2++;
}
