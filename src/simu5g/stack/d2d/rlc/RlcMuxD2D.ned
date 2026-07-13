//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.d2d.rlc;

import simu5g.stack.rlc.RlcMux;

//
// Device-to-Device (D2D) aware version of ~RlcMux. It additionally handles
// D2D mode switch notifications arriving from the MAC layer, dispatching them
// to the TX or RX UM entity of the affected bearer. The D2D statistics are
// currently declared on ~RlcMux (see the note there); this module inherits
// them. Selected by the D2D-capable NICs.
//
simple RlcMuxD2D extends RlcMux
{
    parameters:
        @class("RlcMuxD2D");
}
