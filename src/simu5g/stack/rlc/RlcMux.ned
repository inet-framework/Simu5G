//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.rlc;

//
// Lower-layer RLC packet dispatcher.
//
// Dispatches MAC packets to the correct RX entity or TX entity,
// and collects TX entity output to route to the MAC layer.
//
simple RlcMux like IRlcMux
{
    parameters:
        @class("RlcMux");
        @display("p=100,200;i=block/dispatch");
        string bearerManagementModule = default("^.rrc.bearerManagement");

        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToLowerLayer];
        @statistic[sentPacketToLowerLayer](source="sentPacketToLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);

    gates:
        input macIn;
        output macOut;
        output toRxEntity[];          // dynamic: dispatches to RX entity in gates
        input fromTxEntity[];         // dynamic: receives from TX entity out gates
        output macToTxEntity[];       // dynamic: dispatches MAC SDU requests to TX entity macIn gates
}
