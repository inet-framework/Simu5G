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

package simu5g.stack.mac.amc;

//
// Adaptive Modulation and Coding (AMC) module for LTE.
//
// This module is responsible for selecting the appropriate Modulation and Coding
// Scheme (MCS) based on channel quality feedback (CQI) from UEs. It maintains
// feedback history and computes transmission parameters for each UE.
//
simple LteAmc like ILteAmc
{
    parameters:
        @display("i=block/mac");
        @class("LteAmc");

        // AMC Mode:  "AUTO", "D2D"
        string amcMode @enum(AUTO,D2D) = default("AUTO");

        // resource allocation type ("distributed" or "localized")
        string rbAllocationType @enum(distributed,localized) = default("localized");

        // summary feedback confidence function lower bound
        double summaryLowerBound @unit(s) = default(5ms);

        // summary feedback confidence function upper bound
        double summaryUpperBound @unit(s) = default(20ms);

        // FeedBack Historical Base capacity in DL (number of stored feedback samples per UE)
        int fbhbCapacityDl = default(5);

        // FeedBack Historical Base capacity in UL (number of stored feedback samples per UE)
        int fbhbCapacityUl = default(5);

        // FeedBack Historical Base capacity in D2D (number of stored feedback samples per UE)
        int fbhbCapacityD2D = default(5);

        // wideband CQI generation parameter (0.0 means "use the mean value")
        double cqiWeight = default(0.0);

        // Pilot mode for CQI computation
        string pilotMode @enum(IN_CQI,MAX_CQI,AVG_CQI,MEDIAN_CQI,ROBUST_CQI) = default("ROBUST_CQI");

        // Reference to the MAC module
        string macModule = default("^.mac");

        // Reference to the CellInfo module
        string cellInfoModule;

        // Reference to the Binder module
        string binderModule = default("binder");
}
