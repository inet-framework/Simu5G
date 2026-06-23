//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.simulations.nr.sionna;

import simu5g.simulations.nr.networks.SingleCell_Standalone;
import simu5g.stack.phy.channelmodel.sionna.SionnaManager;

//
// NR standalone single-cell network with a global SionnaManager, so the channel
// models can be set to SionnaChannelModel (Plan A). Identical to
// SingleCell_Standalone otherwise.
//
network SionnaSingleCell extends SingleCell_Standalone
{
    submodules:
        sionnaManager: SionnaManager {
            @display("p=50,341;is=s");
        }
}
