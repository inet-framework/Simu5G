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
// NED-submodule wrapper around Tr36814PathLossModel's propagation formulas
// (3GPP TR 36.814), so the strategy can be a ~CarrierLegPhysics.pathLoss /
// .extCellPathLoss submodule of the medium.
//
simple Tr36814PathLoss like IPathLossModel
{
    parameters:
        @class(Tr36814PathLoss);
        @display("i=block/classifier2");
}
