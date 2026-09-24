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

#include <sstream>

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

const char *rohcStateName(RohcCompressor::State state)
{
    switch (state) {
        case RohcCompressor::IR: return "IR";
        case RohcCompressor::FO: return "FO";
        case RohcCompressor::SO: return "SO";
    }
    return "?";
}

RohcCompressor::RohcCompressor(const Parameters& params) : params_(params)
{
    for (RohcProfile profile : params_.profiles) {
        if (params_.foHeaderSize.count(profile) == 0 || params_.soHeaderSize.count(profile) == 0)
            throw cRuntimeError("RohcCompressor: no compressed header sizes for profile \"%s\"", rohcProfileName(profile));
    }
    if (params_.irPackets < 1 || params_.foPackets < 0 || params_.irRefresh < 1 || params_.foRefresh < 1)
        throw cRuntimeError("RohcCompressor: irPackets, irRefresh and foRefresh must be at least 1, foPackets at least 0");
}

int RohcCompressor::findOrCreateContext(const std::string& flow)
{
    packetCount_++;
    for (int cid = 0; cid < (int)contexts_.size(); cid++) {
        if (contexts_[cid].flow == flow) {
            contexts_[cid].lastUsed = packetCount_;
            return cid;
        }
    }

    // a new flow: a free CID, or the least recently used context, which starts over
    int cid;
    if ((int)contexts_.size() < MAX_CONTEXTS) {
        cid = contexts_.size();
        contexts_.push_back(Context());
    }
    else {
        cid = 0;
        for (int i = 1; i < (int)contexts_.size(); i++)
            if (contexts_[i].lastUsed < contexts_[cid].lastUsed)
                cid = i;
        contexts_[cid] = Context();
    }
    contexts_[cid].flow = flow;
    contexts_[cid].lastUsed = packetCount_;
    return cid;
}

RohcCompressor::State RohcCompressor::nextState(Context& context)
{
    // periodic refreshes: back to IR, or from SO back to FO
    if (context.state != IR && context.packetsSinceIr >= params_.irRefresh) {
        context.state = IR;
        context.packetsInState = 0;
    }
    else if (context.state == SO && context.packetsSinceFo >= params_.foRefresh) {
        context.state = FO;
        context.packetsInState = 0;
    }
    State state = context.state;

    context.packetsSinceIr = state == IR ? 0 : context.packetsSinceIr + 1;
    context.packetsSinceFo = state == SO ? context.packetsSinceFo + 1 : 0;

    // the optimistic approach: a state is left after a number of packets, with no
    // acknowledgment from the decompressor
    context.packetsInState++;
    if (state == IR && context.packetsInState >= params_.irPackets) {
        context.state = params_.foPackets > 0 ? FO : SO;
        context.packetsInState = 0;
    }
    else if (state == FO && context.packetsInState >= params_.foPackets) {
        context.state = SO;
        context.packetsInState = 0;
    }
    return state;
}

RohcCompressor::Result RohcCompressor::compress(Packet *pkt)
{
    auto originalHeaders = makeShared<SequenceChunk>();
    std::ostringstream flow;   // the static header fields

    // the IP header, and what it says about the rest
    int transportProtocol;
    bool isFragment;
    if (&ipProtocolOf(pkt) == &Protocol::ipv4) {
        auto ipv4Header = pkt->removeAtFront<Ipv4Header>();
        transportProtocol = ipv4Header->getProtocolId();
        isFragment = ipv4Header->isFragment();
        flow << ipv4Header->getSrcAddress() << ">" << ipv4Header->getDestAddress();
        ipv4Header->markImmutable();
        originalHeaders->insertAtBack(ipv4Header);
    }
    else {
        // extension headers are chunks of their own: the Next Header field then names the
        // first one, a fragment header included
        auto ipv6Header = pkt->removeAtFront<Ipv6Header>();
        transportProtocol = ipv6Header->getProtocolId();
        isFragment = transportProtocol == IP_PROT_IPv6EXT_FRAGMENT;
        flow << ipv6Header->getSrcAddress() << ">" << ipv6Header->getDestAddress() << "/" << ipv6Header->getFlowLabel();
        ipv6Header->markImmutable();
        originalHeaders->insertAtBack(ipv6Header);
    }
    flow << "/" << transportProtocol;

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
    if (profile == ROHC_UDP || profile == ROHC_RTP) {
        auto udpHeader = pkt->removeAtFront<UdpHeader>();
        flow << "/" << udpHeader->getSrcPort() << ">" << udpHeader->getDestPort();
        moveToOriginalHeaders(udpHeader);
    }
#ifdef INET_WITH_RTP
    if (profile == ROHC_RTP) {
        auto rtpHeader = pkt->removeAtFront<rtp::RtpHeader>();
        flow << "/" << rtpHeader->getSsrc();
        moveToOriginalHeaders(rtpHeader);
    }
#endif
    if (profile == ROHC_TCP) {
        auto tcpHeader = pkt->removeAtFront<tcp::TcpHeader>();
        flow << "/" << tcpHeader->getSrcPort() << ">" << tcpHeader->getDestPort();
        moveToOriginalHeaders(tcpHeader);
    }
    originalHeaders->markImmutable();

    // the flow's context, and the packet's size in the context's state; an uncompressed
    // packet keeps its headers as they are, but it still goes into a RohcHeader and takes
    // a context, as ROHC carries every packet of a bearer that has it configured
    int cid = findOrCreateContext(std::string(rohcProfileName(profile)) + "|" + flow.str());
    State state = SO;
    b compressedSize;
    if (profile == ROHC_UNCOMPRESSED)
        compressedSize = originalHeaders->getChunkLength();
    else {
        state = nextState(contexts_[cid]);
        switch (state) {
            case IR: compressedSize = originalHeaders->getChunkLength() + params_.irOverhead; break;
            case FO: compressedSize = params_.foHeaderSize.at(profile); break;
            case SO: compressedSize = params_.soHeaderSize.at(profile); break;
        }
    }
    if (cid > 0)
        compressedSize += B(1);   // small-CID encoding: add-CID octet

    auto rohcHeader = makeShared<RohcHeader>(originalHeaders, compressedSize);
    rohcHeader->markImmutable();
    pkt->insertAtFront(rohcHeader);
    return Result{profile, cid, state, compressedSize};
}

} // namespace simu5g
