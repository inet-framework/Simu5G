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

package simu5g.stack.sidelink;

import simu5g.stack.NrNicEnb;
import simu5g.stack.sidelink.rrc.SlGnbRrc;

//
// Sidelink-aware gNB NIC (D27, SL-3): a ~NrNicEnb with the gNB-side
// sidelink control plane (~SlGnbRrc, the cell's pool owner) added.
// Selected by the gNodeB's hasSidelink parameter. The Mode-1 scheduling
// MAC variant (NrMacGnbSl) plugs into the mac slot in WP-O.
//
module NrNicEnbSl extends NrNicEnb
{
    submodules:
        //# gNB-side sidelink control plane: cell pool owner + SL-UE registry
        slGnbRrc: SlGnbRrc {
            @display("p=80,60,col;g=left");
        }
}
