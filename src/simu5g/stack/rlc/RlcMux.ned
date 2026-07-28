package simu5g.stack.rlc;

//
// Lower-layer RLC packet dispatcher.
//
// Dispatches MAC packets to the correct RX entity or TX entity,
// and collects TX entity output to route to the MAC layer.
//
simple RlcMux
{
    parameters:
        @class("RlcMux");
        @display("p=100,200;i=block/dispatch");
        string bearerManagementModule = default("^.rrc.bearerManagement");

        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToLowerLayer];
        @statistic[sentPacketToLowerLayer](source="sentPacketToLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);

        //# Cell-level statistics. These aggregate over all the bearers of a cell, so
        //# they are recorded here rather than on the individual bearer entities: the
        //# entity that measures a sample reaches the serving node's mux through the
        //# Binder and emits it there. The per-bearer statistics live on the entities.
        @signal[rlcCellThroughputUl];
        @statistic[rlcCellThroughputUl](title="Cell Throughput at the rlc layer UL"; unit="Bps"; source="rlcCellThroughputUl"; record=simu5g_rateavg);
        @signal[rlcCellThroughputDl];
        @statistic[rlcCellThroughputDl](title="Cell Throughput at the rlc layer DL"; unit="Bps"; source="rlcCellThroughputDl"; record=simu5g_rateavg);
        @signal[rlcCellPacketLossUl];
        @statistic[rlcCellPacketLossUl](title="Cell packet loss at the rlc layer UL"; unit=""; source="rlcCellPacketLossUl"; record=mean);
        @signal[rlcCellPacketLossDl];
        @statistic[rlcCellPacketLossDl](title="Cell packet loss at the rlc layer DL"; unit=""; source="rlcCellPacketLossDl"; record=mean);

    gates:
        input macIn;
        output macOut;
        output toRxEntity[];          // dynamic: dispatches to RX entity in gates
        input fromTxEntity[];         // dynamic: receives from TX entity out gates
        output macToTxEntity[];       // dynamic: dispatches MAC SDU requests to TX entity macIn gates
}
