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

package simu5g.simulations.NR.sionna;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.NrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.stack.phy.channelmodel.sionna.SionnaManager;
import simu5g.world.radio.LteChannelControl;


//
// SionnaSingleLink
//
// Minimal NR single-link network (one gNB + one UE) for the empty-world walking
// skeleton. The gNB and UE are placed at the SSOT positions of the Sionna artifact
// (Tx [0,0,10] m, Rx [100,0,1.5] m) so the in-sim geometry matches the offline
// PathSolver geometry exactly and the empty-world Friis round-trip closes (CAL-01).
//
// A single network-scope ~SionnaManager loads the offline artifact once and asserts
// the parameter contract against the live scenario at init (CAL-02). The UE's
// SionnaChannelModel resolves it via an absolute module path set in omnetpp.ini.
//
network SionnaSingleLink
{
    parameters:
        int numUe = default(1);
        @display("i=block/network2;bgb=400,300");
    submodules:
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,75");
        }
        binder: Binder {
            @display("p=50,125;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50,175;is=s");
        }
        // Loads the offline Sionna artifact once and asserts the contract (CAL-02).
        // The contract values MUST match the artifact manifest (scenario.example.json):
        // carrier 3.5 GHz, SCS 30 kHz, 1 band. A mismatch aborts the run loud.
        sionnaManager: SionnaManager {
            @display("p=50,225;is=s");
            carrierFrequencyHz = default(3.5e9);
            subcarrierSpacingHz = default(30000.0);
            numBands = default(1);
        }
        server: StandardHost {
            @display("p=150,50;is=n;i=device/server");
        }
        router: Router {
            @display("p=250,50;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=350,50");
        }
        gnb: gNodeB {
            @display("p=200,150;is=vl");
        }
        ue[numUe]: NrUe {
            @display("p=300,200");
        }
    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.filterGate;
        upf.pppg++ <--> Eth10G <--> gnb.ppp;
}
