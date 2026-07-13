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

import simu5g.stack.rlc.LteRlcUmEntity;

//
// RLC UM entity of a D2D-capable LTE data radio bearer: ~LteRlcUmEntity with both
// sides bound to the D2D entity profiles. Selected by BearerManagement on the
// D2D-capable NICs via its lteRlcUmEntityModuleType parameter.
//
module LteRlcUmEntityD2D extends LteRlcUmEntity
{
    parameters:
        tx.typename = default("simu5g.stack.d2d.rlc.LteRlcUmTxEntityD2D");
        rx.typename = default("simu5g.stack.d2d.rlc.LteRlcUmRxEntityD2D");
}
