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

package simu5g.stack.pdcp;

//
// Handles operations related to the LTE PDCP (Packet Data Convergence Protocol)
// layer on the transmitting side of a specific connection. This module manages the creation,
// sequencing, header compression, and metadata association for PDCP PDUs before they are passed
// down to the RLC layer.
//
// Header compression (ROHC) is on when the bearer's configuration lists ROHC profiles
// (the rohc field of ~BearerConfigurator's bearer definitions, pushed as rohcProfiles).
// It is a size model: the compressed IP and transport headers travel inside the ROHC
// header, which has the compressed size, and the receiver puts them back. Each packet
// is compressed with the most specific of the bearer's profiles that fits it: "rtp" (an
// RTP header follows the UDP header), "udp", "tcp", then "ip", which covers the IP header
// only and leaves the transport header as it is; a fragment can only use "ip". A packet
// no profile fits goes uncompressed. The compressed header size is that of the profile
// (rohcSoHeaderSizes).
//
simple LtePdcpTxEntity like IPdcpTxEntity
{
    parameters:
        @dynamic(true);
        @display("i=block/layer");
        @signal[receivedPacketFromUpperLayer];
        @signal[sentPacketToLowerLayer];
        @signal[pdcpSduSent];
        string binderModule = default("binder");
        bool emitPerSduSignals = default(true);              // Cleared by a PdcpEntityBase with a splitter: the splitter emits the per-leg statistics
        string rohcProfiles = default("");  // The bearer's ROHC profiles, space-separated, pushed by RRC; "" = no header compression
        object rohcSoHeaderSizes = default({rtp: 3, udp: 3, tcp: 5, ip: 1});  // Compressed header size per ROHC profile, in bytes
        string rlcMode @enum(TM,UM,AM) = default("UM");      // RLC mode of this bearer, pushed by RRC; determines the PDCP header size

    gates:
        input in;
        output out;
}

