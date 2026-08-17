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
// Provides the interface for the network-level module that models the shared
// radio medium: the physical facts and the channel effects -- path loss,
// shadowing, fading and interference -- of the radio links between the
// medium's registered radios.
//
moduleinterface IRadioMedium
{
    @display("i=misc/sun");
}
