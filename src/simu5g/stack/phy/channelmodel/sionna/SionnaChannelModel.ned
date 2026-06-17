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

package simu5g.stack.phy.channelmodel.sionna;

import simu5g.stack.phy.channelmodel.NrChannelModel;


//
// Opt-in channel model that sources path gain from an offline Sionna RT precompute.
// It extends ~NrChannelModel, so it satisfies ILteChannelModel via the parent chain
// (SEAM-01) and is selectable through the NIC's parametric channel-model typename.
//
simple SionnaChannelModel extends NrChannelModel
{
    parameters:
        @class("SionnaChannelModel");
        string sionnaManagerModule = default("^.^.sionnaManager");
        string artifactManifest = default("sionna_artifact/manifest.json");
}
