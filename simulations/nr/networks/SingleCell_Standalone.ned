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
import simu5g.nodes.Upf;
import simu5g.nodes.BackgroundCell;
import simu5g.stack.phy.channelmodel.RadioMedium;
import simu5g.world.radio.LteChannelControl;


// SingleCell_standalone
//
// This network represents the deployment option 3 in 3GPP 38.801
//
network SingleCell_Standalone
{
    parameters:
        // radio endpoint recast E5, §3(d): a single match-anything entry
        // would give the UE's vestigial LTE leg (LteNicBase's own default,
        // Tr36814) the same Tr38901 study as the real NR leg -- and the
        // ASSERT-equality bridge (RadioMedium::addRadio()) makes that
        // mismatch a startup error for every registering radio, including
        // the vestigial one, not merely a fingerprint difference to
        // discover later. Taking the plan's pre-authorized escape path
        // (§3(d) decision point) unconditionally rather than only if the
        // suite showed movement, since the bridge shows it immediately.
        // carrierLeg[0] is the real, traffic-carrying NR leg (what ini
        // Configs override to switch study); carrierLeg[1] is the
        // vestigial LTE leg, always Tr36814 like the endpoint's own
        // untouched default, never traffic-carrying, never overridden in
        // this tree.
        radioMedium.numCarrierLegs = default(2);
        radioMedium.carrierLeg[0].leg = default("NR");
        radioMedium.carrierLeg[0].pathLoss.typename = default("Tr38901PathLoss");
        radioMedium.carrierLeg[1].leg = default("LTE");
        int numUe = default(1);
        int numBgCells = default(0);
        @display("i=block/network2;bgb=1000,1000;bgi=background/pisa");
    submodules:
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125");
        }
        binder: Binder {
            @display("p=50,175;is=s");
        }
        bearerConfigurator: BearerConfigurator {
            @display("p=110,175;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50,258;is=s");
        }
        server: StandardHost {
            @display("p=212,118;is=n;i=device/server");
        }
        router: Router {
            @display("p=363,115;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=527,116");
        }
        iUpf: Upf {
            @display("p=725,118");
        }
        gnb: gNodeB {
            @display("p=726,277;is=vl");
        }
        bgCell[numBgCells]: BackgroundCell {
            @display("p=416,277;is=vl");
        }
        ue[numUe]: NrUe {
            @display("p=628,411");
        }
        radioMedium: RadioMedium {
            @display("p=50,341;is=s");
        }

    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.dnPppg;
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb.ppp;
}
