//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sidelink.mac;

import simu5g.stack.mac.NrMacUe;

//
// SL-aware NR Uu MAC for the UE (D26/D27/D28, SL-3): the Uu side of the
// mode-1 request loop. Bridges to the SL MAC (~NrSlMacUe) via direct C++
// calls: an SL-only mode-1 backlog triggers the preamble RAC, a won RAC
// arms the SL-BSR (a BSR-only PDU with packetLcid SL_SHORT_BSR, the D2D
// pattern), and an arriving ~SlSchedulingGrant is routed to the SL MAC
// without touching Uu grant state. With no mode-1 connections all
// overrides are quiescent (byte-identical to ~NrMacUe).
//
// Selected by the hasSidelink conditional default of the NR-leg mac slot
// in ~NrNicUe.
//
module NrMacUeSl extends NrMacUe
{
    parameters:
        @class("NrMacUeSl");

        string slMacModule = default("^.slMac");   // the sidelink MAC to bridge to
}
