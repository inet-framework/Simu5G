//
//                  Simu5G
//
// Copyright (C) 2026 Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/pdcp/rohc/RohcCompressor.h"

#include <inet/common/packet/chunk/SequenceChunk.h>
#include <inet/networklayer/common/IpProtocolId_m.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include <inet/networklayer/ipv6/Ipv6Header.h>
#include <inet/transportlayer/tcp_common/TcpHeader.h>
#include <inet/transportlayer/udp/UdpHeader_m.h>
#ifdef INET_WITH_RTP
#include <inet/transportlayer/rtp/RtpPacket_m.h>
#endif

#include "simu5g/common/L3Utils.h"
#include "simu5g/stack/pdcp/packet/RohcHeader.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

const char *rohcProfileName(RohcProfile profile)
{
    switch (profile) {
        case ROHC_UNCOMPRESSED: return "uncompressed";
        case ROHC_RTP: return "rtp";
        case ROHC_UDP: return "udp";
        case ROHC_TCP: return "tcp";
        case ROHC_IP: return "ip";
    }
    return "?";
}

const std::vector<std::string>& rohcProfileNames()
{
    static const std::vector<std::string> names = {"rtp", "udp", "tcp", "ip"};
    return names;
}

RohcProfile parseRohcProfile(const std::string& name)
{
    if (name == "rtp") return ROHC_RTP;
    if (name == "udp") return ROHC_UDP;
    if (name == "tcp") return ROHC_TCP;
    if (name == "ip") return ROHC_IP;
    if (name == "esp")
        throw cRuntimeError("ROHC profile \"esp\" is not modeled (available: \"rtp\", \"udp\", \"tcp\", \"ip\")");
    throw cRuntimeError("Unknown ROHC profile \"%s\" (available: \"rtp\", \"udp\", \"tcp\", \"ip\")", name.c_str());
}

RohcCompressor::RohcCompressor(const Parameters& params) : params_(params)
{
    for (RohcProfile profile : params_.profiles)
        if (params_.soHeaderSize.count(profile) == 0)
            throw cRuntimeError("RohcCompressor: no compressed header size for profile \"%s\"", rohcProfileName(profile));
}

RohcProfile RohcCompressor::compress(Packet *pkt)
{
    auto originalHeaders = makeShared<SequenceChunk>();

    // the IP header, and what it says about the rest
    int transportProtocol;
    bool isFragment;
    if (&ipProtocolOf(pkt) == &Protocol::ipv4) {
        auto ipv4Header = pkt->removeAtFront<Ipv4Header>();
        transportProtocol = ipv4Header->getProtocolId();
        isFragment = ipv4Header->isFragment();
        ipv4Header->markImmutable();
        originalHeaders->insertAtBack(ipv4Header);
    }
    else {
        // extension headers are chunks of their own: the Next Header field then names the
        // first one, a fragment header included
        auto ipv6Header = pkt->removeAtFront<Ipv6Header>();
        transportProtocol = ipv6Header->getProtocolId();
        isFragment = transportProtocol == IP_PROT_IPv6EXT_FRAGMENT;
        ipv6Header->markImmutable();
        originalHeaders->insertAtBack(ipv6Header);
    }

    // the most specific allowed profile that fits (a fragment only fits the IP profile)
    RohcProfile profile = ROHC_UNCOMPRESSED;
    if (!isFragment && transportProtocol == IP_PROT_UDP) {
        bool hasRtpHeader = false;
#ifdef INET_WITH_RTP
        if (pkt->getDataLength() > UDP_HEADER_LENGTH)
            hasRtpHeader = dynamicPtrCast<const rtp::RtpHeader>(pkt->peekDataAt(UDP_HEADER_LENGTH)) != nullptr;
#endif
        if (hasRtpHeader && isAllowed(ROHC_RTP))
            profile = ROHC_RTP;
        else if (isAllowed(ROHC_UDP))
            profile = ROHC_UDP;
    }
    else if (!isFragment && transportProtocol == IP_PROT_TCP && isAllowed(ROHC_TCP))
        profile = ROHC_TCP;
    if (profile == ROHC_UNCOMPRESSED && isAllowed(ROHC_IP))
        profile = ROHC_IP;

    // the transport headers the profile covers
    auto moveToOriginalHeaders = [&](const Ptr<Chunk>& header) {
        header->markImmutable();
        originalHeaders->insertAtBack(header);
    };
    if (profile == ROHC_UDP || profile == ROHC_RTP)
        moveToOriginalHeaders(pkt->removeAtFront<UdpHeader>());
#ifdef INET_WITH_RTP
    if (profile == ROHC_RTP)
        moveToOriginalHeaders(pkt->removeAtFront<rtp::RtpHeader>());
#endif
    if (profile == ROHC_TCP)
        moveToOriginalHeaders(pkt->removeAtFront<tcp::TcpHeader>());
    originalHeaders->markImmutable();

    // an uncompressed packet keeps its size; it still goes into a RohcHeader, as ROHC
    // carries every packet of a bearer that has it configured
    b compressedSize = profile == ROHC_UNCOMPRESSED ? originalHeaders->getChunkLength() : b(params_.soHeaderSize.at(profile));
    auto rohcHeader = makeShared<RohcHeader>(originalHeaders, compressedSize);
    rohcHeader->markImmutable();
    pkt->insertAtFront(rohcHeader);
    return profile;
}

} // namespace simu5g
