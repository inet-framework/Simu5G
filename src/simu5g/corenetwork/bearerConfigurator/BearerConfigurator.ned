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

package simu5g.corenetwork.bearerConfigurator;

//
// Central configurator of the network's data radio bearers.
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
// A static bearer is not only configured but also established up front, in the last
// initialization stage: each entry results in the same establishment call that
// packet-triggered setup makes, so traffic finds the bearer in place. A dual-stack UE's
// bearer is established on the stack that packet-triggered establishment would pick
// (split into per-technology legs when the UE is served by a node pair in dual
// connectivity); an entry whose UE is attached to no cell at that point is a
// configuration error. To make the static configuration the only source of bearers,
// disable packet-triggered establishment via the establishBearersOnDemand parameter of
// ~NrSdap (or ~Ip2Nic, for stacks without SDAP); see the VoIP-DL-MultiQfi-NoOnDemand
// configuration of the simulations/nr/standalone_drb example.
//
// Because an entry names its UE by module path rather than by node id, the configuration
// follows the UE from cell to cell, and neither end of the radio link is configured
// directly from the ini.
//
// staticDrbs fields:
//  - coreNetwork (string, required): "5gc" or "epc" -- which bearer architecture selects the
//    bearer. A "5gc" bearer is selected by the QFIs mapped onto it (mappedQfis; needs SDAP in
//    the stack); an "epc" bearer is selected by packet filters (filters; for stacks without
//    SDAP). Stated per entry, never inferred; each receiving RRC checks it against its stack.
//  - ue (string): module path of the UE node, relative to the network; patterns are allowed,
//    so one entry can describe a bearer of many UEs (e.g. "ue[*]")
//  - drbId (int): DRB identifier (= nrPdcp[]/nrRlc[] array index)
//  - profile (string, optional): name of a drbProfiles entry supplying defaults for the
//    fields it does not spell out itself
//  - mappedQfis (int[], optional, "5gc" only): QFI values mapped to this DRB; an entry without
//    it does not take part in SDAP's QFI-to-DRB mapping
//  - filters (string[], optional, "epc" only): packet filters selecting this bearer, each
//    an inet::PacketFilter -- a message-name pattern (e.g. "VoIP*") or an expression written
//    as "expr(...)" (e.g. "expr(udp.destPort == 3000)"). An expression sees the packet that
//    triggers establishment, i.e. an UL-triggered and a DL-triggered flow present swapped
//    port/address roles; a list can carry both spellings.
//  - lcg (int, optional): the bearer's logical channel group (0..3), which the MAC's
//    LCG scheduler serves in increasing-index priority order; omitted = derived from
//    the QoS profile's priority level (bucketed by lcgPriorityBounds), or 0 when the
//    definition carries no "qosPriorityLevel"
//  - rlcMode (string, optional when the definition carries a "packetErrorRate"): RLC
//    mode ("AM", "UM", "TM"); omitted = derived from the QoS profile's packet error rate
//    (packetErrorRate <= amPerThreshold -> "AM", else "UM": a PER target HARQ alone
//    cannot meet gets ARQ).
//    Required, in the entry or its profile, when there is nothing to derive it from.
//  - legSelection (string, optional): which leg of a split bearer carries a given PDU;
//    an expression written as "expr(...)" over typeOfService and packetOrdinal, compiled
//    and evaluated per PDU by the bearer's leg splitter (see ~DcPdcpLegSplitter, whose
//    legSelection parameter this overrides). Only meaningful on a bearer with two legs.
//  - legs (string[]/object[], optional): the cell groups that serve this bearer, i.e. its
//    RLC bearers (TS 38.331 RLC-BearerConfig). One leg = an MCG or an SCG bearer, both =
//    a split bearer. An element is either a cell group name ("MCG" or "SCG"), or an object
//    naming it and overriding the RLC configuration it inherits from the entry:
//    {leg: "SCG", rlcMode: "AM", soFraming: true, snFieldLength: 18}. Omitted = the
//    configuration does not state the legs and RRC derives them, as it always has.
//  - pduSessionType (string, optional): "IPv4", "IPv6", "IPv4v6", "Ethernet", "Unstructured";
//    default: "IPv4"
//  - upperProtocol (string, optional): INET protocol name for upper layer dispatch (this is
//    normally only needed for "Unstructured" PDU sessions); default: derived from
//    pduSessionType
//  - isDefault (bool, optional): marks this as the default DRB for the UE; if no entry of a
//    UE is marked, its first bearer takes the role. The default DRB is the fallback when no
//    QFI-to-DRB mapping ("5gc") or packet filter ("epc") matches.
//  - gbr (bool), packetDelayBudget (double, ms), packetErrorRate (double),
//    qosPriorityLevel (int) -- all optional: the
//    QoS profile of the bearer's flows (5QI characteristics). Any of them present marks the
//    bearer as having a QoS profile, which RRC pushes into the eNB/gNB MAC for QoS-aware
//    scheduling.
//
// Field groups that repeat across entries can be named once in the drbProfiles parameter (a
// JSON map of name -> field group) and referenced from entries via their "profile" field. A
// profile describes what the bearer is, so it may hold any entry field except the ones that
// say which UE it belongs to (ue, drbId), which architecture and flows select it (coreNetwork,
// mappedQfis, filters, isDefault).
//
// The standardized QoS characteristics rows are available as predefined profiles, referenced
// by name without being defined: "qci-1".."qci-9" (TS 23.203 Table 6.1.7-A) and
// "5qi-1".."5qi-9" (TS 23.501 Table 5.7.4-1). A row carries what the spec standardizes --
// gbr, qosPriorityLevel, packetDelayBudget, packetErrorRate -- and nothing else: the
// RLC mode and the logical channel
// group are RAN choices, derived from those characteristics by default (the way a gNB
// derives its AS configuration from the delivered QoS profile; see the rlcMode and lcg
// field descriptions) and stated on the entry only to override, e.g.
// {coreNetwork: "5gc", ue: "ue[*]", drbId: 0, mappedQfis: [1, 2], profile: "5qi-1"}.
// A drbProfiles entry may not redefine a predefined name. NOTE the spec's priority scales
// (QCI 1..9, 5QI 10..90) differ from each other and from the small hand-picked values older
// configurations use; the QoS-aware scheduler weights bearers by 1/(qosPriorityLevel+1), and the
// default lcgPriorityBounds suit the 5QI scale (single-digit priorities all land in LCG 0),
// so profiles from different scales should not be mixed within one network.
//
// Example (in an ini file):
//
// <pre>
// *.bearerConfigurator.drbProfiles = {
//     voice:  {rlcMode: "UM", gbr: true, packetDelayBudget: 100, packetErrorRate: 1e-2,
//              qosPriorityLevel: 2}
//     }
// *.bearerConfigurator.staticDrbs = [
//     {coreNetwork: "5gc", ue: "ue[*]", drbId: 0, mappedQfis: [1, 2], profile: "voice"}
//     ]
// </pre>
//
// The onDemandDrbs parameter describes bearers that are created when traffic first
// matches them, instead of being configured up front. Entries have the same fields as
// staticDrbs minus drbId: an on-demand bearer's id is assigned at creation, so
// definitions can never collide over ids ("ue" may also be omitted, meaning every UE).
// A "5gc" entry serves the QFIs in its mappedQfis: the first packet of an uncovered QFI
// creates the DRB (SDAP asks this module on a QFI-to-DRB lookup miss), and later QFIs
// of the same entry join it. An "epc" entry is matched by its filters against the
// packet that triggers bearer establishment, and an entry with isDefault=true catches
// the flows no filter matched ("5gc" entries cannot be the default: the default DRB is
// where unmapped QFIs go, so it must exist up front). A created bearer's definition is
// delivered to the RRCs involved exactly like a staticDrbs entry. Matching order is
// staticDrbs entries first, then onDemandDrbs entries, each in table order, first
// match wins.
//
// A flow that no definition covers is a configuration error: every on-demand bearer's
// properties come from a definition entry, never from the packet. The parameter's
// default value keeps configurations that author nothing working -- it carries the
// well-known packet-name classes as ordinary catch-all definitions ("VoIP*" = LCG 0,
// "gaming*" = LCG 2, "VoD*" = LCG 1, everything else LCG 3, all RLC UM), so overriding
// the parameter replaces that classification with the configuration's own bearer set.
// An "epc" entry describes bearers of SDAP-less stacks only; a "ue" pattern (or an
// omitted "ue") covering UEs of both kinds simply skips the SDAP ones, but an entry
// whose explicit "ue" pattern matches no UE with a compatible stack is an error.
// D2D and multicast bearers are outside the definition system and are established
// with a fixed transitional configuration (RLC UM, LCG 3) until they get definitions
// of their own.
//
// Example (in an ini file; the first entry suits a stack with SDAP, the other two
// one without):
//
// <pre>
// *.bearerConfigurator.onDemandDrbs = [
//     {coreNetwork: "5gc", ue: "ue[*]", mappedQfis: [3, 4], profile: "gaming"},
//     {coreNetwork: "epc", filters: ["expr(udp.destPort == 3000)"], lcg: 0, rlcMode: "UM"},
//     {coreNetwork: "epc", isDefault: true, lcg: 3, rlcMode: "UM"}
//     ]
// </pre>
//
// See the VoIP-DL-MultiQfi-OnDemandDrb configuration of simulations/nr/standalone_drb
// ("5gc") and the SingleCell-DL-OnDemandDrb configuration of simulations/lte/tutorial
// ("epc") for working setups.
//
simple BearerConfigurator
{
    parameters:
        string binderModule = default("binder");
        object staticDrbs = default([]);  // JSON array: [{coreNetwork, ue, drbId, profile, mappedQfis, filters, lcg, rlcMode, legs, pduSessionType, upperProtocol, isDefault, gbr, packetDelayBudget, packetErrorRate, qosPriorityLevel}, ...]; see banner comment for details
        object onDemandDrbs = default([  // JSON array, staticDrbs schema minus "drbId" (and "ue" optional): bearers created when traffic first matches them; the default value carries the legacy packet-name classification; see banner comment for details
                {coreNetwork: "epc", filters: ["VoIP*"], lcg: 0, rlcMode: "UM"},
                {coreNetwork: "epc", filters: ["gaming*"], lcg: 2, rlcMode: "UM"},
                {coreNetwork: "epc", filters: ["VoDPacket*", "VoDFinishPacket*"], lcg: 1, rlcMode: "UM"},
                {coreNetwork: "epc", isDefault: true, lcg: 3, rlcMode: "UM"}
            ]);
        object drbProfiles = default({});  // JSON map of named field groups referenced by the staticDrbs entries' "profile" field, e.g. {voice: {rlcMode: "UM", gbr: true, ...}, ...}; see banner comment

        // QoS-derived RAN defaults for definitions that do not state rlcMode/lcg themselves
        // (operator policy, not spec-mandated: 3GPP standardizes the QoS characteristics,
        // the RAN chooses what to make of them)
        double amPerThreshold = default(1e-4);  // rlcMode derivation: a definition whose QoS profile has packetErrorRate <= this gets "AM", else "UM"
        object lcgPriorityBounds = default([25, 50, 75]);  // lcg derivation: qosPriorityLevel < bounds[k] -> LCG k, else LCG 3; strictly ascending, one bound between each pair of adjacent LCGs
        @display("i=block/cogwheel");
}
