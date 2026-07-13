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

import simu5g.stack.rlc.um.NrRlcUmRxEntity;

//
// D2D-capable NR (TS 38.322) RLC UM receiving entity: ~NrRlcUmRxEntity plus the
// mode-switch buffer discard/renumbering and the D2D branch of the
// throughput/delay statistics.
//
simple NrRlcUmRxEntityD2D extends NrRlcUmRxEntity
{
    parameters:
        @class("NrRlcUmRxEntityD2D");
}
