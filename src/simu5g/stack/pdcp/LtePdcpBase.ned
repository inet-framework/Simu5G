//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//


package simu5g.stack.pdcp;

//
// The LtePdcpBase module implements the functionalities of the Packet Data
// Convergence Protocol (PDCP). It enables header compression and maps incoming IP packets
// to different Radio Link Control (RLC) modes. This module dynamically creates
// ~LteTxPdcpEntity and ~LteRxPdcpEntity submodules to handle individual transmission/reception connections
//
simple LtePdcpBase like ILtePdcp
{
    parameters:
        @display("i=block/fork");
        string binderModule = default("binder");
        string packetFlowManagerModule = default("^.packetFlowManager"); // optional
        string nrPacketFlowManagerModule = default("^.nrPacketFlowManager"); // optional
        int headerCompressedSize @unit(B) = default(-1B);   // TODO test header compression (headerCompressedSize=3B)!!!   Header compressed size (bytes), -1B = compression disabled

        //# Statistics
        @signal[receivedPacketFromUpperLayer];
        @statistic[receivedPacketFromUpperLayer](source="receivedPacketFromUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[receivedPacketFromLowerLayer];
        @statistic[receivedPacketFromLowerLayer](source="receivedPacketFromLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToUpperLayer];
        @statistic[sentPacketToUpperLayer](source="sentPacketToUpperLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @signal[sentPacketToLowerLayer];
        @statistic[sentPacketToLowerLayer](source="sentPacketToLowerLayer"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
    gates:
        //#
        //# Gates connecting UE/eNB and PDCP/RRC Layer
        //#
        //# Data Ports

        inout DataPort;            // Port to transmit data packets

        //#
        //# Gates connecting PDCP/RRC and RLC Layer
        //#
        //# Data Ports

        output rlcDispatcherOut;   // Output to RLC Mode Dispatcher
        input rlcDispatcherIn;     // Input from RLC Mode Dispatcher
}
