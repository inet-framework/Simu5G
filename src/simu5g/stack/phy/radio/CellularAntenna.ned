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

package simu5g.stack.phy.radio;

import inet.mobility.contract.IMobility;

//
// The antenna of a cellular radio: its gain, whether it radiates in all
// directions or sectorially, towards txAngle, and where it is. The sector's
// pattern is the propagation study's (see the channel model's pathLossType).
//
// The antenna is where its optional mobility submodule says, as an antenna of
// INET is: typically an ~AntennaMobility, which follows the node's mobility at an
// offset, the antenna's height above the node (mobility.offsetZ). The node
// stays on the ground, which is flat, at z = 0. Without a mobility submodule
// the antenna is where its node is.
//
module CellularAntenna
{
    parameters:
        double gain @unit(dBi) = default(0dBi);     // Antenna gain, transmitting and receiving
        string txDirection @enum(ANISOTROPIC,OMNI) = default("OMNI");
        double txAngle @unit(deg) = default(0deg);   // Boresight of a sectorial (ANISOTROPIC) antenna
        mobility.subjectModule = default("");        // The mobility moves no icon
        @class(CellularAntenna);
        @display("i=device/antennatower");
    submodules:
        mobility: <default("")> like IMobility if typename != "" {
            @display("p=100,100;is=s");
        }
}
