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

import simu5g.stack.mac.NrMacGnb;

//
// SL-aware NR gNB MAC (D27, SL-3): intercepts SL-BSR PDUs (packetLcid
// SL_SHORT_BSR) before the Uu BSR machinery, feeds them to ~SlGnbRrc's
// SlEnbScheduler, and returns the resulting ~SlSchedulingGrant on the
// stock GRANTPKT path. Owns no sidelink state itself. Selected by
// ~NrNicEnbSl's mac slot default.
//
module NrMacGnbSl extends NrMacGnb
{
    parameters:
        @class("NrMacGnbSl");

        string slGnbRrcModule = default("^.slGnbRrc");
}
