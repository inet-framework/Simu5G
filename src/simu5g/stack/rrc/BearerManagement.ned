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

package simu5g.stack.rrc;

//
// RRC Bearer Management — creates and tears down PDCP, RLC and MAC
// entities for data radio bearers.
//
simple BearerManagement
{
    parameters:
        @display("i=block/control");
        @class(BearerManagement);

        // PDCP entity parameters
        int headerCompressedSize @unit(B) = default(-1B);

        // PDCP entity types: the full PDCP entity is a compound (PdcpEntity, TX+RX sides plus
        // the leg split/join stage of a multi-leg bearer); variants are its subclasses (e.g.
        // NrPdcpEntity). At a DC secondary a bearer's PDCP lives at the master, so the secondary
        // installs an PdcpRelayEntity (tunnels PDUs to/from the master) in its place.
        string pdcpEntityModuleType = default("simu5g.stack.pdcp.PdcpEntity");
        string pdcpRelayEntityModuleType = default("simu5g.stack.pdcp.PdcpRelayEntity");

        bool dualConnectivityEnabled = default(false);   // set by the NIC; two-leg (split-capable) bearers

        // RLC entity types
        string rlcTmEntityModuleType = default("simu5g.stack.rlc.RlcTmEntity");
        string rlcUmEntityModuleType = default("simu5g.stack.rlc.RlcUmEntity");
        string rlcAmEntityModuleType = default("simu5g.stack.rlc.RlcAmEntity");

        // Module references for accessing other stack layers
        string binderModule = default("binder");
        string registrationModule = default("^.registration");
        string pdcpMuxModule = default("^.^.pdcpMux");
        string pdcpDcMuxModule = default("^.^.pdcpDcMux");
        string d2dModeControllerModule = default("^.d2dModeController");
        string rlcMuxModule = default("^.^.rlcMux");
        string nrRlcMuxModule = default("^.^.nrRlcMux");
        string macModule = default("^.^.mac");
        string nrMacModule = default("^.^.nrMac");
}
