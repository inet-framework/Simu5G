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

#ifndef _L3_UTILS_H_
#define _L3_UTILS_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/common/IpHeaderFieldsTag_m.h"

namespace simu5g {

// Parses the IP header at the front of a user-plane packet and attaches its fields
// as an IpHeaderFieldsTag, replacing any tag already there. Called where the packet
// enters a node's user plane; the node's later modules read the tag.
inet::Ptr<const IpHeaderFieldsTag> attachIpHeaderFields(inet::Packet *pkt);

// The DSCP field of a DSCP and ECN octet (see IpHeaderFieldsTag::tos)
inline uint8_t dscpOf(uint8_t tos) { return (tos & 0xfc) >> 2; }

} // namespace simu5g

#endif
