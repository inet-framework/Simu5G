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

package simu5g.stack.rlc;

//
// Interface for the lower-layer RLC packet dispatcher.
//
// A module implementing this interface dispatches MAC packets to the correct
// RX entity or TX entity, and collects TX entity output to route to the MAC
// layer. The default implementation is ~RlcMux.
//
moduleinterface IRlcMux
{
    gates:
        input macIn;
        output macOut;
        output toRxEntity[];          // dynamic: dispatches to RX entity in gates
        input fromTxEntity[];         // dynamic: receives from TX entity out gates
        output macToTxEntity[];       // dynamic: dispatches MAC SDU requests to TX entity macIn gates
}
