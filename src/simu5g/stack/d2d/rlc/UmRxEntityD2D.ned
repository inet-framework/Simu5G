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

import simu5g.stack.rlc.um.UmRxEntity;

//
// Device-to-Device (D2D) aware version of ~UmRxEntity. D2D_MULTI connections
// use a single-PDU reordering window initialized on the first received PDU;
// after a D2D mode switch, sequence numbering is reset and the next PDU is
// treated as in-sequence; statistics of D2D flows are emitted on the D2D
// signals. Created by BearerManagement on the D2D-capable NICs.
//
simple UmRxEntityD2D extends UmRxEntity
{
    parameters:
        @class("UmRxEntityD2D");
}
