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

package simu5g.stack.pdcp;

//
// Dual Connectivity Mux: handles X2/DC routing between PDCP entities
// and the DcX2Forwarder.
//
simple DcMux
{
    parameters:
        @display("i=block/dispatch");
        string binderModule = default("binder");
        string bearerManagementModule = default("^.rrc.bearerManagement");
    gates:
        input dcManagerIn @loose;
        output dcManagerOut @loose;
        input fromEntity[];       // from a secondary's PDCP relay (UL) or a master leg's remote side (DL out)
        output toRelayEntity[];   // to a secondary's PDCP relay (DL, PdcpRelayEntity)
        output toRxEntity[];      // to a master PDCP entity's remote (X2) leg (UL in)
}
