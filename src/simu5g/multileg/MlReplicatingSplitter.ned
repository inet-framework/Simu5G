//
// Multileg demo project for Simu5G (see package.ned).
//
package simu5g.multileg;

import simu5g.stack.pdcp.IPdcpLegSplitter;

//
// FRER-style replicating leg dispatcher: sends a copy of every PDCP PDU on
// every connected leg, each addressed to that leg's node ids (see
// MlReplicatingSplitter.h). The replicas share the PDCP SN assigned
// upstream, so the receiver's reorder window eliminates the duplicates.
//
simple MlReplicatingSplitter like IPdcpLegSplitter
{
    parameters:
        int numLegs;                          // number of legs (set by the enclosing PdcpEntityBase)
        int firstStackLeg = default(1);       // bearer leg 0 rides this stack leg
        string binderModule = default("binder");
        @display("i=block/dispatch");
        @signal[sentPacketToLowerLayer];
        @signal[replicaSent];
        @statistic[replicaSent](title="PDCP PDU replicas sent"; record=count);
    gates:
        input in;               // PDUs from the TX entity
        output out[];           // to leg k
}
