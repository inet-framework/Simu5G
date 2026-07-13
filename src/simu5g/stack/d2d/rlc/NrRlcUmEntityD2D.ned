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

import simu5g.stack.rlc.NrRlcUmEntity;

//
// RLC UM entity of a D2D-capable NR data radio bearer: ~NrRlcUmEntity with both
// sides bound to the D2D entity profiles. Selected by BearerManagement on the
// D2D-capable NICs via its nrRlcUmEntityModuleType parameter.
//
module NrRlcUmEntityD2D extends NrRlcUmEntity
{
    parameters:
        tx.typename = default("simu5g.stack.d2d.rlc.NrRlcUmTxEntityD2D");
        rx.typename = default("simu5g.stack.d2d.rlc.NrRlcUmRxEntityD2D");
}
