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

import inet.common.INETDefs;
import inet.common.TagBase;
import inet.networklayer.common.L3Address;

namespace simu5g;

//
// The fields of a user-plane packet's IP header that a node classifies and
// routes by, parsed once where the packet enters the node's user plane (see
// attachIpHeaderFields()) and read by every later module of the node instead of
// the header itself, so a missing entry point aborts rather than silently
// re-parsing.
//
// The tag is node-local: across a node boundary only the header bytes carry this
// information, so every entry point parses afresh and replaces any tag already
// present. removeAllSimu5GTags() leaves it in place.
//
class IpHeaderFieldsTag extends inet::TagBase {
    inet::L3Address srcAddress;
    inet::L3Address destAddress;
    uint8_t tos;   // The DSCP and ECN octet: IPv4 Type of Service, IPv6 Traffic Class
}
