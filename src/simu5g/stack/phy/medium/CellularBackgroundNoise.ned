//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.medium;

import inet.physicallayer.wireless.common.contract.packetlevel.IBackgroundNoise;

// SPIKE: a background noise that adds none (the channel models' thermal noise is Simu5G's)
module CellularBackgroundNoise like IBackgroundNoise
{
    parameters:
        @class(CellularBackgroundNoise);
}
