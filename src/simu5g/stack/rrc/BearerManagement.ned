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

        // PDCP entity types: the full PDCP entity is a compound (PdcpEntityBase, TX+RX sides plus
        // the leg split/join stage of a multi-leg bearer); the instantiable types are its
        // subclasses (LtePdcpEntity, NrPdcpEntity). At a DC secondary a bearer's PDCP lives at
        // the master, so the secondary installs an PdcpRelayEntity (tunnels PDUs to/from the
        // master) in its place.
        string pdcpEntityModuleType = default("simu5g.stack.pdcp.LtePdcpEntity");
        string pdcpRelayEntityModuleType = default("simu5g.stack.pdcp.PdcpRelayEntity");

        bool dualConnectivityEnabled = default(false);   // set by the NIC; two-leg (split-capable) bearers

        // RLC entity types (compound modules): lteRlc* = LTE-FI bearers (TS 36.322),
        // nrRlc* = NR bearers (SI/SO wire format, TS 38.322)
        string rlcTmEntityModuleType = default("simu5g.stack.rlc.RlcTmEntity");
        string lteRlcUmEntityModuleType = default("simu5g.stack.rlc.LteRlcUmEntity");
        string lteRlcAmEntityModuleType = default("simu5g.stack.rlc.LteRlcAmEntity");
        string nrRlcUmEntityModuleType = default("simu5g.stack.rlc.NrRlcUmEntity");
        string nrRlcAmEntityModuleType = default("simu5g.stack.rlc.NrRlcAmEntity");

        // RLC entity types for the NR leg (SI/SO byte-offset wire format, TS 38.322).
        // Default to the Nr* profiles so an NR-leg bearer uses NR RLC; a config may
        // point these back to the LTE entities above to keep an NR leg on LTE RLC
        // (e.g. nr/mec, whose RNI PacketFlowObserver still tracks the LTE-FI UM PDU).
        // TM is transparent and identical for both RATs, so it has no NR override.
        string nrRlcUmTxEntityModuleType = default("simu5g.stack.rlc.um.NrUmTxEntity");
        string nrRlcUmRxEntityModuleType = default("simu5g.stack.rlc.um.NrUmRxEntity");
        string nrRlcAmTxEntityModuleType = default("simu5g.stack.rlc.am.NrAmTxQueue");
        string nrRlcAmRxEntityModuleType = default("simu5g.stack.rlc.am.NrAmRxQueue");

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
