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
import inet.networklayer.configurator.contract.IL3NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.corenetwork.bearerConfigurator.BearerConfigurator;
import simu5g.corenetwork.coreControl.CoreControl;
import simu5g.nodes.NrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.stack.phy.medium.CellularRadioMedium;

//
// A three-sector site in an NR standalone deployment: three gNBs on one mast
// (gnb1, gnb2, gnb3), each with a sectorial antenna, connected to a 5G core
// and to each other over X2, and UEs around the site. The ini file gives the
// three cells one site name (cellularNic.radio.site), which makes them the
// sectors of one site for the channel model: a UE's links to the three share
// one LOS state and one shadowing realization, and differ by the antenna
// patterns only.
//
network ThreeSectorSite_Standalone
{
    parameters:
        radioMedium.numCarriers = carrierAggregation.numComponentCarriers;
        // The gNBs connect to a 5GC (Upf), so every cellular stack has SDAP:
        // TS 37.324 ties SDAP to the 5GC connection, not to the radio
        **.cellularNic.hasSdap = default(true);
        int numUe = default(1);
        @display("i=block/network2;bgb=1300,700");
    submodules:
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125;is=s");
        }
        ipv6Configurator: <default("")> like IL3NetworkConfigurator if typename != "" {
            @display("p=50,325;is=s");
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
            @display("p=212,118;is=n;i=device/server");
        }
        router: Router {
            @display("p=351,115;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=351,215");
        }
        iUpf: Upf {
            @display("p=351,315");
        }
        // the three sectors of the site: three cells on one mast
        gnb1: gNodeB {
            @display("p=520,330;is=vl");
        }
        gnb2: gNodeB {
            @display("p=480,330;is=vl");
        }
        gnb3: gNodeB {
            @display("p=500,365;is=vl");
        }
        ue[numUe]: NrUe {
            @display("p=500,428");
        }
        radioMedium: CellularRadioMedium {
            @display("p=50,100;is=s");
        }
    connections:

        //# Data Network connections
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.dnPppg;

        //# 5G Core Network connections
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb3.ppp;

        //# X2 connections
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb1.x2++ <--> Eth10G <--> gnb3.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;
}
