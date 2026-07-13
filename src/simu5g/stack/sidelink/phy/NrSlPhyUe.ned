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

package simu5g.stack.sidelink.phy;

import simu5g.stack.phy.LtePhyBase;

//
// Sidelink PHY of a UE: SlAirFrame fan-out over the slRadioIn gate chain,
// store-and-decode-at-slot-end reception, half-duplex bookkeeping.
// WP-C: ideal decode (no SINR/BLER); TR 37.885 channel model in WP-D.
//
simple NrSlPhyUe extends LtePhyBase
{
    parameters:
        @class(NrSlPhyUe);
        @display("i=block/wrxtx");

        string slRrcModule = default("^.slRrc");

        // fan-out pruning: skip receivers farther than this; <=0 disables the check
        double slTxRange @unit(m) = default(-1m);
}
