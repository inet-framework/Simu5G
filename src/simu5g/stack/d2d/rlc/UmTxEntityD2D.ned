//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//


package simu5g.stack.d2d.rlc;

import simu5g.stack.rlc.um.UmTxEntity;

//
// Device-to-Device (D2D) aware version of ~UmTxEntity. It adds the machinery
// needed by D2D mode switching: while the RLC entity associated with the old
// mode drains its TX buffer, incoming SDUs are parked in a holding queue and
// released once the drain completes (coordinated by the D2DModeController).
// Created by BearerManagement on the D2D-capable NICs.
//
simple UmTxEntityD2D extends UmTxEntity
{
    parameters:
        @class("UmTxEntityD2D");
}
