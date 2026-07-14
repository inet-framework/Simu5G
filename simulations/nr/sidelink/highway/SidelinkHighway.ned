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
// The highway is a gently curved arc of a circle of radius 4000 m centered
// at (1000, 4150): vehicles drive on it with ~CircleMobility (lane l uses
// radius 3990+4l; the inner and outer three lanes carry the two
// directions), and the road drawing below uses arcs of the same circle.
// A vehicle at mobility angle theta sits at figure angle 90deg-(theta-270deg).
//
network SidelinkHighway
{
    parameters:
        int numUes = default(170);
        @display("i=block/network2;bgb=2300,400,#b8d4a8");

        // the road: asphalt body, white edge lines, dashed lane separators
        // and the center divider, all arcs of the mobility circle
        @figure[road](type=arc; bounds=-3000,150,8000,8000; startAngle=72; endAngle=108; lineColor=#4a4a4a; lineWidth=26; zoomLineWidth=true);
        @figure[edgeS](type=arc; bounds=-2988,162,7976,7976; startAngle=72; endAngle=108; lineColor=white; lineWidth=0.7; zoomLineWidth=true);
        @figure[laneS1](type=arc; bounds=-2992,158,7984,7984; startAngle=72; endAngle=108; lineColor=white; lineStyle=dashed; lineWidth=0.5; zoomLineWidth=true);
        @figure[laneS2](type=arc; bounds=-2996,154,7992,7992; startAngle=72; endAngle=108; lineColor=white; lineStyle=dashed; lineWidth=0.5; zoomLineWidth=true);
        @figure[center](type=arc; bounds=-3000,150,8000,8000; startAngle=72; endAngle=108; lineColor=#e8c860; lineWidth=1; zoomLineWidth=true);
        @figure[laneN1](type=arc; bounds=-3004,146,8008,8008; startAngle=72; endAngle=108; lineColor=white; lineStyle=dashed; lineWidth=0.5; zoomLineWidth=true);
        @figure[laneN2](type=arc; bounds=-3008,142,8016,8016; startAngle=72; endAngle=108; lineColor=white; lineStyle=dashed; lineWidth=0.5; zoomLineWidth=true);
        @figure[edgeN](type=arc; bounds=-3012,138,8024,8024; startAngle=72; endAngle=108; lineColor=white; lineWidth=0.7; zoomLineWidth=true);

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
