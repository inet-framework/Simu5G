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


package simu5g.stack.mac;

//
// Extends the ~LteMacBase module by providing specific MAC functions
// on the eNodeB side. Specifically, it initializes and manages the scheduling classes
// responsible for resource allocation operations in both uplink and downlink directions,
// while also supporting Carrier Aggregation (CA).
// It manages the Random Access procedure (RAC) and the reception of Buffer Status
// Reports (BSRs) from UEs, as well as the transmission of scheduling grants to UEs allocated
// within the uplink bandwidth. For downlink connections, it notifies the Radio Link Control (RLC)
// layer when it is ready to receive MAC Service Data Units (SDUs) to build MAC
// Protocol Data Units (PDUs) and transmit them to the physical layer.
//
simple LteMacEnb extends LteMacBase
{
    parameters:
        @class("LteMacEnb");

        //# volatile xml optSolution = default(xmldoc("solution.sol"));

        // number of eNodeBs - set to 0 if unknown
        int eNodeBCount = default(0);

        // Reference to the AMC module
        string amcModule = default("^.amc");

        //#
        //# eNb Scheduler Parameters
        //#
        // Scheduling discipline. See LteCommon.h for discipline meaning.
        string schedulingDisciplineDl @enum(DRR,PF,MAXCI,MAXCI_MB,MAXCI_OPT_MB,MAXCI_COMP,ALLOCATOR_BESTFIT) = default("MAXCI");
        string schedulingDisciplineUl @enum(DRR,PF,MAXCI,MAXCI_MB,MAXCI_OPT_MB,MAXCI_COMP,ALLOCATOR_BESTFIT) = default("MAXCI");

        // Proportional Fair parameters
        double pfAlpha = default(0.95);

        string cellInfoModule;
        string rlcUmModule = default("^.rlc.um");
        string pdcpModule = default("^.pdcp");

        //#
        //# Statistics related to resource blocks occupancy
        @signal[avgServedBlocksDl];
        @statistic[avgServedBlocksDl](title="Average number of allocated Resource Blocks in the Dl"; unit="blocks"; source="avgServedBlocksDl"; record=mean,vector);
        @signal[avgServedBlocksUl];
        @statistic[avgServedBlocksUl](title="Average number of allocated Resource Blocks in the Dl"; unit="blocks"; source="avgServedBlocksUl"; record=mean,vector);
}

