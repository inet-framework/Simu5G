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
package simu5g.simulations.lte.networks;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.corenetwork.bearerConfigurator.BearerConfigurator;
import simu5g.corenetwork.coreControl.CoreControl;
import simu5g.nodes.PgwStandard;
import simu5g.nodes.LteUe;
import simu5g.nodes.eNodeB;
import simu5g.stack.phy.medium.CellularRadioMedium;

//
// A three-sector site in an LTE deployment: three eNodeBs on one mast
// (eNodeB1, eNodeB2, eNodeB3), each with a sectorial antenna, connected to an
// EPC and to each other over X2, and UEs around the site. The ini file gives
// the three cells one site name (cellularNic.radio.site), which makes them
// the sectors of one site for the channel model: a UE's links to the three
// share one LOS state and one shadowing realization, and differ by the
// antenna patterns only.
//
network ThreeSectorSite
{
    parameters:
        radioMedium.numCarriers = carrierAggregation.numComponentCarriers;
        int numUe = default(1);
        @display("i=block/network2;bgb=1300,700");
    submodules:
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125;is=s");
        }
        binder: Binder {
            @display("p=50,175;is=s");
        }
        coreControl: CoreControl {
            @display("p=50,375;is=s");
        }
        bearerConfigurator: BearerConfigurator {
            @display("p=50,225;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50,275;is=s");
        }
        server: StandardHost {
            @display("p=173,48;is=n;i=device/server");
        }
        pgw: PgwStandard {
            @display("p=172,287;is=l");
        }
        router1: Router {
            @display("p=256,286;i=device/smallrouter");
        }
        router2: Router {
            @display("p=295,135;i=device/smallrouter");
        }
        router3: Router {
            @display("p=295,453;i=device/smallrouter");
        }
        eNodeB1: eNodeB {
            @display("p=356,287;is=vl");
        }
        eNodeB2: eNodeB {
            @display("p=578,136;is=vl");
        }
        eNodeB3: eNodeB {
            @display("p=578,454;is=vl");
        }
        ue[numUe]: LteUe {
            @display("p=421,293");
        }
        radioMedium: CellularRadioMedium {
            @display("p=50,100;is=s");
        }
    connections:

        server.pppg++ <--> Eth10G <--> pgw.dnPppg;
        pgw.pppg++ <--> Eth10G <--> router1.pppg++;
        pgw.pppg++ <--> Eth10G <--> router2.pppg++;
        pgw.pppg++ <--> Eth10G <--> router3.pppg++;
        router1.pppg++ <--> Eth10G <--> eNodeB1.ppp;
        router2.pppg++ <--> Eth10G <--> eNodeB2.ppp;
        router3.pppg++ <--> Eth10G <--> eNodeB3.ppp;

        //# X2 connections
        eNodeB1.x2++ <--> Eth10G <--> eNodeB2.x2++;
        eNodeB1.x2++ <--> Eth10G <--> eNodeB3.x2++;
        eNodeB2.x2++ <--> Eth10G <--> eNodeB3.x2++;
}
