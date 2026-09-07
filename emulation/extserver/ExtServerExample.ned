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

package simu5g.emulation.extserver;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.corenetwork.smf.Smf;
import simu5g.nodes.ExtCell;
import simu5g.nodes.NrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.stack.phy.channelmodel.RadioMedium;
import simu5g.world.radio.LteChannelControl;


// ExtServerExample
//
// One simulated UE receives data from a real server, and one simulated
// UE sends data to a real server
//
//
network ExtServerExample
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
        int numExtCells = default(0);
        @display("i=block/network2;bgb=800,554;bgi=background/pisa");
    submodules:
        routingRecorder: RoutingTableRecorder {
            @display("p=73,130;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=73,72");
        }
        channelControl: LteChannelControl {
            @display("p=70,190;is=s");
        }
        binder: Binder {
            @display("p=73,261;is=s");
        }
        smf: Smf {
            @display("p=133,261;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=72,335;is=s");
        }
        router: Router {
            @display("p=223,175;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=460,174");
        }
        gnb: gNodeB {
            @display("p=671,174;is=vl");
        }
        ue1: NrUe {
            @display("p=700,309");
        }
        ue2: NrUe {
            @display("p=636,309");
        }
        extCell[numExtCells]: ExtCell {
            @display("p=671,432;is=vl");
        }
        radioMedium: RadioMedium {
            @display("p=72,418;is=s");
        }

    connections allowunconnected:
        router.pppg++ <--> Eth10G <--> upf.dnPppg;
        upf.pppg++ <--> Eth10G <--> gnb.ppp;
}
