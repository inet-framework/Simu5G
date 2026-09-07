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
// NED-submodule wrapper around Tr38901PathLossModel's propagation formulas
// (3GPP TR 38.901), so the strategy can be a ~CarrierLegPhysics.pathLoss /
// .extCellPathLoss submodule of the medium (radio endpoint recast E5). See
// Tr36814PathLoss for the full rationale, shared by all three wrappers.
//
simple Tr38901PathLoss like IPathLossModel
{
    parameters:
        @class(Tr38901PathLoss);
        @display("i=block/classifier2");

        // Enable the high-loss building penetration model (table 7.4.3-2 in
        // TR 38.901); the only study-specific parameter this study needs
        // beyond ~CarrierLegPhysics's shared geometry. RadioMedium reads it
        // and calls Tr38901PathLossModel::setUseBuildingPenetrationHighLossModel()
        // when this leg's pathLoss study is Tr38901.
        bool useBuildingPenetrationHighLossModel = default(false);
}
