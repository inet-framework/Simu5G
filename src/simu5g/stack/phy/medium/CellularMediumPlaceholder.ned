//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.phy.medium;

//
// An empty submodule of ~CellularRadioMedium that reserves the place of one
// of the submodules INET's radio medium has (propagation, analog model,
// background noise, path loss, medium limit cache, communication cache).
// Nothing uses it; it keeps the medium's module tree the shape of INET's.
//
module CellularMediumPlaceholder
{
    parameters:
        @display("i=block/blackboard;is=s");
}
