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


package simu5g.common.binder;

//
// Binder module
//
// The Binder module functions as a global repository of network-related information
// utilized throughout the simulation for various control-plane functionalities. It provides
// a centralized storage and access point for data such as network configurations and
// operational parameters.
// Only one instance of this module is permitted within the entire network, ensuring
// the consistency and integrity of the shared information.
//
// The staticBearers parameter establishes data radio bearers up front, in the last
// initialization stage, instead of on the first packet of a flow. Each entry describes
// one bearer between a UE and its serving node (established as duplex, and split into
// two legs when the UE is served by a node pair in dual connectivity), and results in
// the same establishment call that packet-triggered setup makes. Entry fields:
//  - ue (string): module path of the UE node, relative to the network (e.g. "ue[0]")
//  - qosClass (string, optional): "CONVERSATIONAL", "STREAMING", "INTERACTIVE" or
//    "BACKGROUND"; determines the logical channel group, and the RLC mode when
//    rlcType is omitted; default: "CONVERSATIONAL"
//  - rlcType (string, optional): "TM", "UM" or "AM"; omitted = RRC decides from qosClass
//  - drb (int, optional): DRB identity of the bearer; useful for matching the DRB ids
//    of the SDAP drbConfig parameter; omitted = the next free id for the node pair
//  - technology (string, optional): "LTE" or "NR", selects the stack of a dual-stack
//    UE; omitted = the NR stack if the UE is attached with it, except when serving
//    nodes are in a dual-connectivity setup (where establishment splits the bearer
//    into per-technology legs itself)
//
simple Binder
{
    parameters:
        int blerShift = default(0);
        double maxDataRatePerRb @unit("Mbps") = default(1.16Mbps);
        bool printTrafficGeneratorConfig = default(false);
        object staticBearers = default([]);  // JSON array: [{ue, qosClass, rlcType, drb, technology}, ...]; see banner comment for details
        @display("i=block/cogwheel");
}
