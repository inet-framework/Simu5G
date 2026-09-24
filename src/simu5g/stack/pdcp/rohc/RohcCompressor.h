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
 * The ROHC compressor of one PDCP entity (a size model: the removed headers travel
 * along inside the RohcHeader chunk, and the decompressor puts them back).
 *
 * Every packet is compressed with the most specific of the bearer's profiles that fits
 * it: RTP if an RTP header follows the UDP header, then UDP, TCP, and IP, which covers
 * the IP header only. A fragment can only use the IP profile. A packet no configured
 * profile fits goes uncompressed (profile 0x0000), and costs no extra bytes.
 */
class RohcCompressor
{
  public:
    struct Parameters {
        std::set<RohcProfile> profiles;                // the bearer's configured profiles
        std::map<RohcProfile, inet::B> soHeaderSize;   // compressed header size per profile
    };

  protected:
    Parameters params_;

    virtual bool isAllowed(RohcProfile profile) const { return params_.profiles.count(profile) != 0; }

  public:
    explicit RohcCompressor(const Parameters& params);
    virtual ~RohcCompressor() {}

    // Replaces the headers of the IP datagram at the front of the packet that its profile
    // covers with a RohcHeader of the compressed size, which carries them. Returns the
    // profile used.
    virtual RohcProfile compress(inet::Packet *pkt);
};

} // namespace simu5g

#endif
