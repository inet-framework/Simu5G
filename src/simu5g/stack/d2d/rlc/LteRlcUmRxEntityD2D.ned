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

import simu5g.stack.rlc.um.LteRlcUmRxEntity;

//
// D2D-capable LTE (TS 36.322) RLC UM receiving entity: ~LteRlcUmRxEntity plus the
// D2D_MULTI single-PDU reordering window, the post-mode-switch reassembly
// resynchronisation, and the D2D branch of the throughput/delay statistics.
//
simple LteRlcUmRxEntityD2D extends LteRlcUmRxEntity
{
    parameters:
        @class("LteRlcUmRxEntityD2D");
}
