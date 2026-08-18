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
// Computes interference for the radio medium: the received power contributed
// by transmissions other than the one being evaluated.
//
simple CellularInterferenceModel like IInterferenceModel
{
    parameters:
        @class(CellularInterferenceModel);
        @display("i=block/classifier2");

        string binderModule = default("binder");
}
