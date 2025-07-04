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

package simu5g.stack.rlcModeDispatcher;

//
// RLC Mode Dispatcher Module
//
// This module handles the dispatch of packets to appropriate RLC modes (AM, UM, TM)
// based on traffic classification and configuration. It factors out the RLC mode
// selection logic from the PDCP layer to provide better separation of concerns.
//
simple RlcModeDispatcher
{
    parameters:
        @display("i=misc/node_vs");

        // RLC type configuration for different traffic classes
        string conversationalRlc @enum("TM","UM","AM") = default("AM");
        string interactiveRlc @enum("TM","UM","AM") = default("AM");
        string streamingRlc @enum("TM","UM","AM") = default("UM");
        string backgroundRlc @enum("TM","UM","AM") = default("AM");

    gates:
        // Gates to/from PDCP
        input pdcpIn;
        output pdcpOut;

        // RLC SAP gates (matching RLC interface)
        inout TM_Sap[];
        inout UM_Sap[];
        inout AM_Sap[];
}
