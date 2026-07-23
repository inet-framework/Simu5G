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


package simu5g.stack.phy;

//
// Extends the ~LtePhyBase module by providing the specific PHY functions
// on the eNodeB side. It is responsible for checking if a frame received on the air interface
// originates from a User Equipment (UE) connected to the eNB itself, and it provides the
// functions necessary to compute the Channel Status Information (CSI) for the UEs connected to it.
//
simple LtePhyEnb extends LtePhyBase
{
    parameters:
        @class("LtePhyEnb");

        // Marks this eNB as a handover target: its beacons are used by UEs for handover
        // feasibility assessment. Requires enableBeacons=true (the default).
        bool enableHandover = default(false);

        // Enables broadcasting periodic beacons (at cellInfo.beaconInterval). Beacons are the
        // measurement signal UEs use both for handover feasibility assessment and for radio
        // link failure (RLF) detection. On by default so radio link monitoring works out
        // of the box; set to false to suppress beacons entirely. Must stay true when
        // enableHandover=true. Requires cellInfo.beaconInterval > 0.
        bool enableBeacons = default(true);

        double targetBler = default(0.001);
}

