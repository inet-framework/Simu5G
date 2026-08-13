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
// Per-UE sidelink control plane: owns the sidelink preconfiguration and
// creates SLRB entity chains via ~BearerManagement. Broadcast/groupcast
// establishment stays genie (C++ calls); PC5 unicast links are established
// either by the genie handshake (default) or over the air (D23, WP-L):
// real PC5-RRC messages on a reserved TM SL-SRB (DRB 63), with the first
// data packet held until the link is ESTABLISHED.
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

        // PC5 unicast link establishment: "genie" = synchronous C++ handshake;
        // "overTheAir" = real PC5-RRC messages over the TM SL-SRB (D23) --
        // establishment latency becomes observable, the triggering packet is
        // held until the link is up. The SRB chains themselves bootstrap via
        // the genie mechanism (documented simplification: SRB0-3 as a single
        // pre-provisioned control bearer).
        string pc5RrcMode @enum(genie,overTheAir) = default("genie");

        string bearerManagementModule = default("^.rrc.bearerManagement");
        string ip2nicModule = default("^.ip2nic");

    gates:
        input srbIn[];     // PC5-RRC messages from the per-peer TM SL-SRB RX chains
        output srbOut[];   // PC5-RRC messages to the per-peer TM SL-SRB TX chains
}
