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

package simu5g.stack.d2d.mac;

import simu5g.stack.mac.NrMacUe;

//
// NR-UE MAC with D2D support. Currently a placeholder that adds nothing to
// ~NrMacUe; it will host the NR-UE D2D deltas once ~NrMacUe is re-parented
// onto ~LteMacUe.
//
module NrMacUeD2D extends NrMacUe
{
    parameters:
        @class("NrMacUeD2D");
}
