//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//


package simu5g.stack;

import simu5g.stack.sdap.INrSdap;
import simu5g.stack.sdap.common.IQfiContextManager;

//
// This module extends the Enb NIC to insert SDAP functionality (receiver-side only)
// between IP and PDCP layers, allowing QoS Flow management per 3GPP 5G specifications.
//
module NRNicEnbSdap extends NrNicEnb
{
    parameters:
        hasNrSdap = true;
        int numDrbs = 1;
        string qfiContextFile = default("");  // Path to QFI context configuration file
        mac.qfiContextManagerModule = default("^.qfiContextManager");
        @display("bgb=694,443");

    submodules:

        qfiContextManager: <default("QfiContextManager")> like IQfiContextManager {
            qfiContextFile = parent.qfiContextFile;
            @display("p=100,67;is=s");
        }

        sdap: <default("NrSdap")> like INrSdap {
            numDrbs = parent.numDrbs;
            nodeRole = "gNB";
            qfiContextManagerModule = "^.qfiContextManager";
            @display("p=245,67");
        }

    connections:
        ip2nic.stackNic <--> sdap.DataPort;

        //# SDAP <-> PDCP
        sdap.stackSdap <--> pdcp.DataPort;
}
