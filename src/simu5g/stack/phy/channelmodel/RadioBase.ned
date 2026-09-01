//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.channelmodel;

//
// This is the base module for radios that model the radio channel
// realistically (as opposed to ~IdealRadio). It primarily provides the
// initialization of parameters such as the reference to the Component
// Carrier (CC) and its associated parameters.
//
simple RadioBase like IRadio
{
    parameters:
        @display("i=block/classifier2");
        @class("RadioBase");

        string binderModule = default("binder");
        string cellInfoModule = default("^.^.cellInfo");  // optional

        // component carriers this radio serves, space-separated, resolved in
        // declaration order: the order the
        // cellInfo_->registerCarrier / binder_->registerCarrierUe sweeps register in
        string componentCarrierModules = default("carrierAggregation.componentCarrier[0]");
}

