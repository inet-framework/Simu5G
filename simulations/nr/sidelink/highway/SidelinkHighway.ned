//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//
package simu5g.simulations.nr.sidelink.highway;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.NrUe;
import simu5g.stack.sidelink.common.SlStatsCollector;
import simu5g.world.radio.LteChannelControl;


//
// TR 37.885 highway calibration scenario: a 2 km, 6-lane highway of
// out-of-coverage sidelink UEs broadcasting CAM-style periodic messages;
// the slStatsCollector submodule records PRR/PIR per distance bin.
//
network SidelinkHighway
{
    parameters:
        int numUes = default(170);
        @display("i=block/network2;bgb=600,400");

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
        carrierAggregation: CarrierAggregation {
            @display("p=50,225;is=s");
        }
        slStatsCollector: SlStatsCollector {
            @display("p=50,275;is=s");
        }
        ue[numUes]: NrUe {
            @display("p=300,200,row,100");
        }
}
