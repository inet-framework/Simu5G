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
// Provides the interface for the per-carrier-leg propagation-formula
// strategy modules of ~CarrierLegPhysics: path loss, LOS probability,
// shadowing standard deviation and antenna-pattern attenuation for one 3GPP
// propagation study (TR 36.814, TR 36.873 or TR 38.901). The medium resolves
// one such submodule per matched carrier leg (RadioMedium::addRadio()) and
// calls it for every link on that leg; the submodule itself never draws on
// its own rng-0 -- all propagation randomness
// comes out of the owning ~RadioMedium's own rng-0 stream.
//
moduleinterface IPathLossModel
{
    @display("i=block/classifier2");
}
