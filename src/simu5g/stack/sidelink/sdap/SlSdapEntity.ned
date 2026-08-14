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

package simu5g.stack.sidelink.sdap;

//
// Per-destination sidelink SDAP entity (design decision D20): maps PC5 QoS
// flows (PFI) to SLRBs. One dynamic instance per peer and direction, created
// by BearerManagement when the first SLRB toward that peer is established,
// wired as a side chain off ~SlIp2Nic (the pdcpMux wiring stays untouched).
//
//  - TX role (keyed by the destination L2 ID): receives the classified SL
//    packet with its resolved PFI (QfiReq tag), picks the serving SLRB from
//    its slice of slrbConfig / the link's SLRBs (entry pfi match, else the
//    isDefault entry, else the first), stamps drbId/rlcType/slPqi on the
//    FlowControlInfo and returns the packet.
//  - RX role (keyed by the source pid): stamps the QfiInd tag from the
//    delivering SLRB (future home of reflective QoS / per-flow state).
//
simple SlSdapEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/classifier");

        string role @enum(tx,rx);
        int peerKey;                        // TX: destination L2 ID; RX: source pid
        string slRrcModule = default("^.slRrc");

    gates:
        input in;      // packet from SlIp2Nic
        output out;    // packet back to SlIp2Nic
}
