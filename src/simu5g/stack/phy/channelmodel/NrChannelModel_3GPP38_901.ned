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

package simu5g.stack.phy.channelmodel;


//
// ~LteRealisticChannelModel preset with the propagation formulas of
// 3GPP TR 38.901, "Study on channel model for frequencies from 0.5 to 100 GHz",
// v16.1.0, December 2019. It differs from its base type only in the
// pathLossType default; there is no C++ class behind the name. It is the
// default channel model of the gNodeB NIC and of the NR side of the NR UE NIC.
//
simple NrChannelModel_3GPP38_901 extends NrChannelModel
{
    parameters:
        pathLossType = default("Tr38901");
}
