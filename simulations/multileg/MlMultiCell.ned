//
// Multileg demo project for Simu5G (see simu5g.multileg's package.ned).
//
package simu5g.simulations.multileg;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.multileg.MlBinder;
import simu5g.multileg.MlNrUe;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.world.radio.LteChannelControl;

//
// Two standalone gNBs and UEs with an extra NR leg each: leg 1 serves via
// gnb1, leg 2 via gnb2. The MlBinder replaces the stock Binder (a network is
// the only place the binder's type can be chosen).
//
network MlMultiCell
{
    parameters:
        int numUe = default(1);
        @display("i=block/network2;bgb=1088,678");
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
        binder: MlBinder {
            @display("p=50,175;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50,258;is=s");
        }
        server: StandardHost {
            @display("p=212,118;is=n;i=device/server");
        }
        router: Router {
            @display("p=351,115;i=device/smallrouter");
        }
        upf: Upf {
            @display("p=500,115");
        }
        iUpf: Upf {
            @display("p=500,215");
        }
        gnb1: gNodeB {
            @display("p=250,300;is=vl");
        }
        gnb2: gNodeB {
            @display("p=750,300;is=vl");
        }
        ue[numUe]: MlNrUe {
            @display("p=500,428");
        }
    connections:

        //# Data Network connections
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.dnPppg;

        //# 5G Core Network connections
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf.pppg++ <--> Eth10G <--> gnb2.ppp;

        //# X2 connections (unused: no DC; kept for topology parity)
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
}
