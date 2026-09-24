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

#ifndef _ROHC_COMPRESSOR_H_
#define _ROHC_COMPRESSOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <inet/common/packet/Packet.h>

namespace simu5g {

// The ROHC profiles the model implements (RFC 3095, RFC 3843, RFC 6846)
enum RohcProfile {
    ROHC_UNCOMPRESSED,  // 0x0000: none of the bearer's profiles fits the packet
    ROHC_RTP,           // 0x0001: IP + UDP + RTP headers
    ROHC_UDP,           // 0x0002: IP + UDP headers
    ROHC_TCP,           // 0x0006: IP + TCP headers
    ROHC_IP             // 0x0004: the IP header only
};

// The name of a profile in the bearer configuration ("rtp", "udp", "tcp", "ip")
const char *rohcProfileName(RohcProfile profile);

// The profile with the given configuration name; throws for an unknown or unmodeled one
RohcProfile parseRohcProfile(const std::string& name);

// The configuration names of the profiles a bearer can be configured with, most specific first
const std::vector<std::string>& rohcProfileNames();

/**
 * The ROHC compressor of one PDCP entity, in the unidirectional mode (U-mode, RFC 3095
 * 5.3), which needs no feedback from the decompressor. A size model: the removed headers
 * travel along inside the RohcHeader chunk, and the decompressor puts them back.
 *
 * Every packet is compressed with the most specific of the bearer's profiles that fits
 * it: RTP if an RTP header follows the UDP header, then UDP, TCP, and IP, which covers
 * the IP header only. A fragment can only use the IP profile. A packet no configured
 * profile fits goes uncompressed (profile 0x0000).
 *
 * Each flow (a profile and the header fields that do not change: addresses, protocol,
 * ports, RTP SSRC) has a context, identified by a CID. A bearer has at most 16 contexts
 * (CIDs 0..15, the small-CID encoding): CID 0 costs nothing, the others one byte per
 * packet. A new flow finding every CID taken takes over the least recently used context.
 *
 * A context starts in the IR (initialization and refresh) state, whose packets carry the
 * full headers plus irOverhead; after irPackets of them it moves to FO (first order), and
 * after foPackets FO packets to SO (second order), the steady state. The compressor goes
 * back to IR every irRefresh packets and to FO every foRefresh packets of a context, so
 * that a decompressor that lost its context recovers. Uncompressed packets carry their
 * headers as they are.
 *
 * Not modeled: context damage after losses (decompression failures until the next
 * refresh), the O-mode and R-mode feedback, and ROHCv2.
 */
class RohcCompressor
{
  public:
    enum State { IR, FO, SO };

    struct Parameters {
        std::set<RohcProfile> profiles;                // the bearer's configured profiles
        std::map<RohcProfile, inet::B> foHeaderSize;   // compressed header size per profile, FO state
        std::map<RohcProfile, inet::B> soHeaderSize;   // compressed header size per profile, SO state
        inet::B irOverhead = inet::B(3);               // IR packet: packet type, profile, CRC
        int irPackets = 3;                             // IR packets before moving to FO
        int foPackets = 3;                             // FO packets before moving to SO
        int irRefresh = 1700;                          // packets of a context between IR refreshes
        int foRefresh = 700;                           // packets of a context between FO refreshes
    };

    static const int MAX_CONTEXTS = 16;

    struct Result {
        RohcProfile profile;
        int cid;
        State state;
        inet::b compressedSize;
    };

  protected:
    struct Context {
        std::string flow;          // the profile and the static header fields
        State state = IR;
        int packetsInState = 0;    // since the context entered its state
        int packetsSinceIr = 0;
        int packetsSinceFo = 0;
        uint64_t lastUsed = 0;
    };

    Parameters params_;
    std::vector<Context> contexts_;   // indexed by CID
    uint64_t packetCount_ = 0;

    virtual bool isAllowed(RohcProfile profile) const { return params_.profiles.count(profile) != 0; }

    // The CID of the flow's context, creating it (in IR) if there is none
    virtual int findOrCreateContext(const std::string& flow);

    // The state the context's next packet is sent in; advances the context
    virtual State nextState(Context& context);

  public:
    explicit RohcCompressor(const Parameters& params);
    virtual ~RohcCompressor() {}

    // Replaces the headers of the IP datagram at the front of the packet that its profile
    // covers with a RohcHeader of the compressed size, which carries them
    virtual Result compress(inet::Packet *pkt);

    int getNumContexts() const { return contexts_.size(); }
};

const char *rohcStateName(RohcCompressor::State state);

} // namespace simu5g

#endif
