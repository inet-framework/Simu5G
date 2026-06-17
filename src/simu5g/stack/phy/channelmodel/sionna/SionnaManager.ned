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


//
// Loads the offline Sionna artifact once and asserts the parameter contract against
// the live scenario, failing loud on mismatch. Placed once at NIC or network scope;
// ~SionnaChannelModel instances acquire the loaded table from it.
//
simple SionnaManager
{
    parameters:
        @class("SionnaManager");
        string artifactManifest = default("sionna_artifact/manifest.json");
}
