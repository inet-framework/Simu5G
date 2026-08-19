//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.corenetwork.smf;

//
// Session Management Function (SMF) of the core network.
//
// The session management of the core network is what decides which data radio bearers
// exist, what they look like, and when they are set up: it configures the RAN, and the
// RAN never authors a bearer of its own. This module is where that role lives.
//
// Only one instance of this module is permitted within the entire network.
//
// The staticDrbs parameter is the network's data radio bearer configuration: one entry per
// bearer per UE, describing the bearer before and independent of its establishment (modeled
// on the DRB-ToAddMod and RLC-BearerConfig records of TS 38.331). This module tells the RRC
// of each node involved in a bearer -- the UE and its serving node -- what to set up, and
// RRC pushes on what each of its layers consumes. Nothing reads this configuration back out.
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
//  - legSelection (string, optional): which leg of a split bearer carries a given PDU;
//    an expression written as "expr(...)" over typeOfService and packetOrdinal, compiled
//    and evaluated per PDU by the bearer's leg splitter (see ~DcPdcpLegSplitter, whose
//    legSelection parameter this overrides). Only meaningful on a bearer with two legs.
//  - legs (string[]/object[], optional): the cell groups that serve this bearer, i.e. its
//    RLC bearers (TS 38.331 RLC-BearerConfig). One leg = an MCG or an SCG bearer, both =
//    a split bearer. An element is either a cell group name ("MCG" or "SCG"), or an object
//    naming it and overriding the RLC configuration it inherits from the entry:
//    {leg: "SCG", rlcType: "AM", soFraming: true, snFieldLength: 18}. Omitted = the
//    configuration does not state the legs and RRC derives them, as it always has.
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
// *.smf.drbProfiles = {
//     voice:  {rlcType: "UM", gbr: true, delayBudget: 100, per: 1e-2, priority: 2}
//     }
// *.smf.staticDrbs = [
//     {bearerType: "5gc", ue: "ue[*]", drb: 0, qfiList: [1, 2], profile: "voice"}
//     ]
// </pre>
//
// The onDemandDrbs parameter describes bearers that are created when traffic first
// matches them, instead of being configured up front. Entries have the same fields as
// staticDrbs minus drb: an on-demand bearer's id is assigned at creation, so
// definitions can never collide over ids ("ue" may also be omitted, meaning every UE).
// A "5gc" entry serves the QFIs in its qfiList: the first packet of an uncovered QFI
// creates the DRB (SDAP asks this module on a QFI-to-DRB lookup miss), and later QFIs
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
// *.smf.onDemandDrbs = [
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
// *.smf.staticBearers = [
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
simple Smf
{
    parameters:
        string binderModule = default("binder");
        object staticDrbs = default([]);  // JSON array: [{bearerType, ue, drb, profile, qfiList, filters, qosClass, rlcType, legs, pduSessionType, upperProtocol, isDefault, gbr, delayBudget, per, priority}, ...]; see banner comment for details
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
