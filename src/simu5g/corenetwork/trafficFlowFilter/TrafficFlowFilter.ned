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

package simu5g.corenetwork.trafficFlowFilter;

//
// Maps IP 4-Tuples to Traffic Flow Template (TFT) identifiers, which are used by the ~GtpUser module
// contained in the same node. It interacts with the ~Binder to determine the destination of the packet.
// Resides in any module that also includes a ~GtpUser module, such as ~eNodeB, ~gNodeB, ~PgwStandard,
// ~Upf, and ~MecHost modules. For instance, in a ~Upf module acting as the entry point of the 5G network, it
// identifies the destination endpoint of the GTP tunnel that the packet should traverse, which is the gNodeB
// serving the destination User Equipment (UE).
//
// The module also assigns each packet entering the tunnel its QoS Flow Identifier
// (QFI), from the qfiRules parameter: a JSON array of rules, evaluated in order,
// first match wins. Rule fields:
//  - filter (string, optional): an inet::PacketFilter -- a message-name pattern
//    (e.g. "*VoIP*") or an expression written as "expr(...)"
//    (e.g. "expr(udp.destPort == 3000)"); omitted = the rule matches every packet
//  - qfi (int, 0..63) or dscpAsQfi (bool): the QFI to assign -- a fixed value, or
//    the packet's IPv4 DSCP field read as the QFI (exactly one of the two)
// A packet matching no rule gets QFI 0 (the default flow). The parameter's default
// value, [{dscpAsQfi: true}], is plain DSCP-as-QFI assignment.
//
// The rules model the packet detection and QoS enforcement rules (PDR/QER) that the
// SMF installs into a UPF over N4 signaling at session setup. A real UPF holds no
// classification policy of its own, so a locally configured rule table is a modeling
// shortcut: the ini author plays the SMF. Each instance classifies the traffic
// entering the tunnel at its own node -- downlink traffic at the core network's
// gateway, uplink traffic at a base station -- so rules are written from that
// instance's perspective.
//
simple TrafficFlowFilter
{
    parameters:
        @display("i=block/filter");

        string binderModule = default("binder");
        string ownerType @enum(ENODEB,GNODEB,PGW,UPF,UPF_MEC); // must be one between ENODEB,GNODEB,PGW,UPF,UPF_MEC
        bool fastForwarding = default(true);
        object qfiRules = default([{dscpAsQfi: true}]);  // JSON array of QFI-assignment rules [{filter, qfi|dscpAsQfi}, ...], first match wins; see banner comment
    gates:
        input internetFilterGateIn;
        output gtpUserGateOut;
}
