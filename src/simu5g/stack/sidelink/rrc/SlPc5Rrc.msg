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

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import simu5g.common.LteCommon;

namespace simu5g;

//
// PC5-RRC message set (design decision D23, WP-L): the over-the-air unicast
// link establishment handshake, riding the reserved TM SL-SRB (DRB id 63).
// Message sizes are byte-count abstractions of the ASN.1 encodings.
//
class SlPc5RrcMessage extends inet::FieldsChunk
{
    MacNodeId initiatorId = NODEID_NONE;   // the link initiator
    MacNodeId responderId = NODEID_NONE;   // the link peer
}

//
// RRCReconfigurationSidelink analog: proposes the link's SLRBs (DRB ids
// allocated by the initiator, D17) with their QoS shape.
//
class SlLinkEstablishRequest extends SlPc5RrcMessage
{
    chunkLength = inet::B(16);      // base size; +6B per SLRB set by the sender
    uint16_t drbIds[];
    unsigned short rlcTypes[];      // LteRlcType
    int pfis[];
    int pqis[];
    bool isDefaults[];
}

//
// RRCReconfigurationCompleteSidelink analog.
//
class SlLinkEstablishResponse extends SlPc5RrcMessage
{
    chunkLength = inet::B(10);
    bool accepted = true;
}

//
// Explicit link release (no automatic trigger in SL-2: no RLF/keepalive).
//
class SlLinkRelease extends SlPc5RrcMessage
{
    chunkLength = inet::B(10);
}
