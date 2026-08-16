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
// The drbConfig parameter is the network's data radio bearer configuration: one entry per
// bearer per UE, describing the bearer before and independent of its establishment (modeled
// on the DRB-ToAddMod and RLC-BearerConfig records of TS 38.331). The Binder acts on it the
// way the core network's session management (the SMF) does: it tells the RRC of each node
// involved in a bearer -- the UE and its serving node -- what to set up, and RRC pushes on
// what each of its layers consumes. Nothing reads this configuration back out of the Binder.
//
// Because an entry names its UE by module path rather than by node id, the configuration
// follows the UE from cell to cell, and neither end of the radio link is configured
// directly from the ini.
//
// drbConfig fields:
//  - ue (string): module path of the UE node, relative to the network; patterns are allowed,
//    so one entry can describe a bearer of many UEs (e.g. "ue[*]")
//  - drb (int): DRB identifier (= nrPdcp[]/nrRlc[] array index)
//  - profile (string, optional): name of a drbProfiles entry supplying defaults for the
//    fields it does not spell out itself
//  - qfiList (int[], optional): QFI values mapped to this DRB; an entry without it does not
//    take part in SDAP's QFI-to-DRB mapping
//  - rlcType (string, optional): RLC mode ("AM", "UM", "TM"); omitted = RRC decides from the
//    traffic class (see the conversationalRlc/... parameters of ~BearerManagement)
//  - pduSessionType (string, optional): "IPv4", "IPv6", "IPv4v6", "Ethernet", "Unstructured";
//    default: "IPv4"
//  - upperProtocol (string, optional): INET protocol name for upper layer dispatch (this is
//    normally only needed for "Unstructured" PDU sessions); default: derived from
//    pduSessionType
//  - isDefault (bool, optional): marks this as the default DRB for the UE; if no entry of a
//    UE is marked, its first bearer takes the role. The default DRB is the fallback when no
//    QFI-to-DRB mapping matches.
//  - gbr (bool), delayBudget (double, ms), per (double), priority (int) -- all optional: the
//    QoS profile of the bearer's flows (5QI characteristics). Any of them present marks the
//    bearer as having a QoS profile, which RRC pushes into the eNB/gNB MAC for QoS-aware
//    scheduling.
//
// Field groups that repeat across entries can be named once in the drbProfiles parameter (a
// JSON map of name -> field group) and referenced from entries via their "profile" field. A
// profile describes what the bearer is, so it may hold any entry field except the ones that
// say which UE it belongs to (ue, drb) or which flows select it (qfiList, isDefault).
//
// Example (in an ini file):
//
// <pre>
// *.binder.drbProfiles = {
//     "voice":  {"rlcType": "UM", "gbr": true, "delayBudget": 100, "per": 1e-2, "priority": 2}
//     }
// *.binder.drbConfig = [
//     {"ue": "ue[*]", "drb": 0, "qfiList": [1, 2], "profile": "voice"}
//     ]
// </pre>
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
//    of the drbConfig parameter of RRC's DrbTable; omitted = the next free id for the
//    node pair
//  - technology (string, optional): "LTE" or "NR", selects the stack of a dual-stack
//    UE; omitted = the NR stack if the UE is attached with it, except when serving
//    nodes are in a dual-connectivity setup (where establishment splits the bearer
//    into per-technology legs itself)
//
// Example (in an ini file):
//
// <pre>
// *.binder.staticBearers = [
//     {"ue": "ue[0]", "drb": 0, "rlcType": "UM"},
//     {"ue": "ue[1]", "drb": 1, "qosClass": "INTERACTIVE", "rlcType": "AM"}
//     ]
// </pre>
//
// To make the static configuration the only source of bearers, disable
// packet-triggered establishment via the establishBearersOnDemand parameter of
// ~NrSdap (or ~Ip2Nic, for stacks without SDAP). See the VoIP-DL-MultiQfi-StaticBearers
// configuration of the simulations/nr/standalone_drb example.
//
simple Binder
{
    parameters:
        int blerShift = default(0);
        double maxDataRatePerRb @unit("Mbps") = default(1.16Mbps);
        bool printTrafficGeneratorConfig = default(false);
        object drbConfig = default([]);  // JSON array: [{ue, drb, profile, qfiList, rlcType, pduSessionType, upperProtocol, isDefault, gbr, delayBudget, per, priority}, ...]; see banner comment for details
        object drbProfiles = default({});  // JSON map of named field groups referenced by the drbConfig entries' "profile" field, e.g. {"voice": {"rlcType": "UM", "gbr": true, ...}, ...}; see banner comment
        object staticBearers = default([]);  // JSON array: [{ue, qosClass, rlcType, drb, technology}, ...]; see banner comment for details
        @display("i=block/cogwheel");
}
