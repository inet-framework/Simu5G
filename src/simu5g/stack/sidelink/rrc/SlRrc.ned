//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sidelink.rrc;

import simu5g.stack.rrc.IRrc;

//
// Per-UE sidelink control plane ("genie" PC5-RRC, phase SL-1): owns the
// sidelink preconfiguration and creates SLRB entity chains via
// ~BearerManagement. Control plane by C++ calls only; over-the-air SL-SRBs
// are a later fidelity upgrade (SL-2).
//
simple SlRrc like IRrc
{
    parameters:
        @display("i=block/control");

        // Sidelink preconfiguration: resource pool + static SLRB config.
        // Example:
        //   { carrierFrequency: 5.9GHz, numerologyIndex: 1,
        //     subchannelSize: 10, numSubchannels: 5,
        //     t2: 20, rsrpThresholdDbm: -128,
        //     slrbConfig: [ { dstL2Id: 0xFFFFFF, castType: "broadcast", drb: 1,
        //                     rlcType: "UM", destAddress: "224.0.0.10" } ] }
        object preconfig = default({});

        // this UE's 24-bit source Layer-2 ID; -1 = derive from the NR node id
        int srcL2Id = default(-1);

        string bearerManagementModule = default("^.rrc.bearerManagement");
}
