//
// Multileg demo project for Simu5G (see simu5g.multileg's package.ned).
//
package simu5g.simulations.multileg;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth10G;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.multileg.MlBinder;
import simu5g.multileg.MlNrUe3;
import simu5g.nodes.gNodeB;
import simu5g.nodes.Upf;
import simu5g.world.radio.LteChannelControl;

//
// FRER scenario: one gNB and UEs with three NR legs, all three attached to
// that gNB. A bearer's PDUs are replicated over all three legs and
// eliminated by the receiver's PDCP reorder window.
//
// All legs terminate at the same gNB because replication and elimination
// must share one PDCP entity pair: PDCP sequence numbers do not survive the
// core network, so legs anchored at different gNBs could not be eliminated
// without the DC/X2 machinery this project deliberately avoids.
//
network MlFrerCell
{
    parameters:
        int numUe = default(1);
        @display("i=block/network2;bgb=1088,678");
    submodules:
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
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
            @display("p=500,300;is=vl");
        }
        ue[numUe]: MlNrUe3 {
            @display("p=500,428");
        }
    connections:
        server.pppg++ <--> Eth10G <--> router.pppg++;
        router.pppg++ <--> Eth10G <--> upf.dnPppg;
        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gnb1.ppp;
}
