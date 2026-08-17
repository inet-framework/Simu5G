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
// The staticDrbs parameter is the network's data radio bearer configuration: one entry per
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
// staticDrbs fields:
//  - bearerType (string, required): "5gc" or "eps" -- which bearer architecture selects the
//    bearer. A "5gc" bearer is selected by the QFIs mapped onto it (qfiList; needs SDAP in
//    the stack); an "eps" bearer is selected by packet filters (filters; for stacks without
//    SDAP). Stated per entry, never inferred; each receiving RRC checks it against its stack.
//  - ue (string): module path of the UE node, relative to the network; patterns are allowed,
//    so one entry can describe a bearer of many UEs (e.g. "ue[*]")
//  - drb (int): DRB identifier (= nrPdcp[]/nrRlc[] array index)
//  - profile (string, optional): name of a drbProfiles entry supplying defaults for the
//    fields it does not spell out itself
//  - qfiList (int[], optional, "5gc" only): QFI values mapped to this DRB; an entry without
//    it does not take part in SDAP's QFI-to-DRB mapping
//  - filters (string[], optional, "eps" only): packet filters selecting this bearer, each
//    an inet::PacketFilter -- a message-name pattern (e.g. "VoIP*") or an expression written
//    as "expr(...)" (e.g. "expr(udp.destPort == 3000)"). An expression sees the packet that
//    triggers establishment, i.e. an UL-triggered and a DL-triggered flow present swapped
//    port/address roles; a list can carry both spellings.
//  - qosClass (string, optional, "eps" only): "CONVERSATIONAL", "STREAMING", "INTERACTIVE"
//    or "BACKGROUND"; the bearer's traffic class, which establishment turns into the
//    logical channel group; default: "CONVERSATIONAL"
//  - rlcType (string, optional): RLC mode ("AM", "UM", "TM"); omitted = RRC decides from the
//    traffic class (see the conversationalRlc/... parameters of ~BearerManagement)
//  - pduSessionType (string, optional): "IPv4", "IPv6", "IPv4v6", "Ethernet", "Unstructured";
//    default: "IPv4"
//  - upperProtocol (string, optional): INET protocol name for upper layer dispatch (this is
//    normally only needed for "Unstructured" PDU sessions); default: derived from
//    pduSessionType
//  - isDefault (bool, optional): marks this as the default DRB for the UE; if no entry of a
//    UE is marked, its first bearer takes the role. The default DRB is the fallback when no
//    QFI-to-DRB mapping ("5gc") or packet filter ("eps") matches.
//  - gbr (bool), delayBudget (double, ms), per (double), priority (int) -- all optional: the
//    QoS profile of the bearer's flows (5QI characteristics). Any of them present marks the
//    bearer as having a QoS profile, which RRC pushes into the eNB/gNB MAC for QoS-aware
//    scheduling.
//
// Field groups that repeat across entries can be named once in the drbProfiles parameter (a
// JSON map of name -> field group) and referenced from entries via their "profile" field. A
// profile describes what the bearer is, so it may hold any entry field except the ones that
// say which UE it belongs to (ue, drb), which architecture and flows select it (bearerType,
// qfiList, filters, isDefault).
//
// Example (in an ini file):
//
// <pre>
// *.binder.drbProfiles = {
//     voice:  {rlcType: "UM", gbr: true, delayBudget: 100, per: 1e-2, priority: 2}
//     }
// *.binder.staticDrbs = [
//     {bearerType: "5gc", ue: "ue[*]", drb: 0, qfiList: [1, 2], profile: "voice"}
//     ]
// </pre>
//
// The onDemandDrbs parameter describes bearers that are created when traffic first
// matches them, instead of being configured up front. Entries have the same fields as
// staticDrbs minus drb: an on-demand bearer's id is assigned at creation, so
// definitions can never collide over ids ("ue" may also be omitted, meaning every UE).
// A "5gc" entry serves the QFIs in its qfiList: the first packet of an uncovered QFI
// creates the DRB (SDAP asks the Binder on a QFI-to-DRB lookup miss), and later QFIs
// of the same entry join it. An "eps" entry is matched by its filters against the
// packet that triggers bearer establishment, and an entry with isDefault=true catches
// the flows no filter matched ("5gc" entries cannot be the default: the default DRB is
// where unmapped QFIs go, so it must exist up front). A created bearer's definition is
// delivered to the RRCs involved exactly like a staticDrbs entry. Matching order is
// staticDrbs entries first, then onDemandDrbs entries, each in table order, first
// match wins.
//
// Example (in an ini file; the first entry suits a stack with SDAP, the other two
// one without):
//
// <pre>
// *.binder.onDemandDrbs = [
//     {bearerType: "5gc", ue: "ue[*]", qfiList: [3, 4], profile: "gaming"},
//     {bearerType: "eps", filters: ["expr(udp.destPort == 3000)"], qosClass: "CONVERSATIONAL", rlcType: "UM"},
//     {bearerType: "eps", isDefault: true, qosClass: "BACKGROUND"}
//     ]
// </pre>
//
// See the VoIP-DL-MultiQfi-OnDemandDrb configuration of simulations/nr/standalone_drb
// ("5gc") and the SingleCell-DL-OnDemandDrb configuration of simulations/lte/tutorial
// ("eps") for working setups.
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
//    of the staticDrbs parameter; omitted = the next free id for the node pair
//  - technology (string, optional): "LTE" or "NR", selects the stack of a dual-stack
//    UE; omitted = the NR stack if the UE is attached with it, except when serving
//    nodes are in a dual-connectivity setup (where establishment splits the bearer
//    into per-technology legs itself)
//
// Example (in an ini file):
//
// <pre>
// *.binder.staticBearers = [
//     {ue: "ue[0]", drb: 0, rlcType: "UM"},
//     {ue: "ue[1]", drb: 1, qosClass: "INTERACTIVE", rlcType: "AM"}
//     ]
// </pre>
//
// To make the static configuration the only source of bearers, disable
// packet-triggered establishment via the establishBearersOnDemand parameter of
// ~NrSdap (or ~Ip2Nic, for stacks without SDAP). See the VoIP-DL-MultiQfi-StaticBearers
// configuration of the simulations/nr/standalone_drb example.
//
// The trafficClassRules parameter authors the traffic class of on-demand bearers that
// no bearer definition covers: flows in configurations that author no definitions, and
// D2D and multicast bearers, which definitions never describe. Unlike a definition, a
// rule authors a property only -- each flow still gets its own bearer, and nothing is
// created or delivered ahead of establishment.
// A JSON array of rules, evaluated in order, first match wins; a flow matching no rule
// is BACKGROUND (the class of the non-GBR default bearer). Rule fields:
//  - filter (string, optional): an inet::PacketFilter -- a message-name pattern
//    (e.g. "VoIP*") or an expression written as "expr(...)"; omitted = matches all
//  - qosClass (string): "CONVERSATIONAL", "STREAMING", "INTERACTIVE" or "BACKGROUND"
// The parameter's default value classifies by well-known packet-name prefixes
// ("VoIP*" = conversational, "gaming*" = interactive, "VoD*" = streaming); override
// it -- e.g. with [] -- to turn that heuristic off.
//
simple Binder
{
    parameters:
        int blerShift = default(0);
        double maxDataRatePerRb @unit("Mbps") = default(1.16Mbps);
        bool printTrafficGeneratorConfig = default(false);
        object staticDrbs = default([]);  // JSON array: [{bearerType, ue, drb, profile, qfiList, filters, qosClass, rlcType, pduSessionType, upperProtocol, isDefault, gbr, delayBudget, per, priority}, ...]; see banner comment for details
        object onDemandDrbs = default([]);  // JSON array, staticDrbs schema minus "drb" (and "ue" optional): bearers created when traffic first matches them; see banner comment for details
        object drbProfiles = default({});  // JSON map of named field groups referenced by the staticDrbs entries' "profile" field, e.g. {voice: {rlcType: "UM", gbr: true, ...}, ...}; see banner comment
        object staticBearers = default([]);  // JSON array: [{ue, qosClass, rlcType, drb, technology}, ...]; see banner comment for details
        object trafficClassRules = default([  // fallback classification of on-demand bearers no definition covers, [{filter, qosClass}, ...], first match wins; see banner comment
                {filter: "VoIP*", qosClass: "CONVERSATIONAL"},
                {filter: "gaming*", qosClass: "INTERACTIVE"},
                {filter: "VoDPacket*", qosClass: "STREAMING"},
                {filter: "VoDFinishPacket*", qosClass: "STREAMING"}
            ]);
        @display("i=block/cogwheel");
}
