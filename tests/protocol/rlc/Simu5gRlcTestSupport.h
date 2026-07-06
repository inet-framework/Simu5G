//
// Simu5G RLC conformance suite -- C++ content predicates for the INET protocol
// test framework (tests/protocol/lib).
//
// The framework's PacketFilter *string* engine (".packet(\"expr\")") can only reach
// whole-content chunks or protocol headers that have a registered ProtocolDissector.
// Simu5G registers no RLC dissector, and the NR data-PDU classes (NrRlcUmDataPdu,
// NrRlcAmDataPdu) are hand-written with no generated cClassDescriptor, so their fields
// are not reflectable from a string expression. This header provides the escape hatch:
// C++ predicates that peek the concrete chunk directly with peekAtFront<T>(), used from a
// test via  .match([](const MatchContext& c){ return rlctest::nrUmIsSegment(c); }).
//
// This mirrors inet/tests/protocol/wifi/WifiTestSupport.h, which does the same for the
// opaque Ieee80211ModeReq PHY-mode tag.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef SIMU5G_TESTS_PROTOCOL_RLC_SIMU5GRLCTESTSUPPORT_H
#define SIMU5G_TESTS_PROTOCOL_RLC_SIMU5GRLCTESTSUPPORT_H

#include "ProtocolTest.h"                                   // MatchContext, PacketEvent

#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"            // LteRlcUmDataPdu, LteRlcAmPdu, FramingInfo, StatusPduData
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"     // LteRlcNewDataTag (MAC new-data notification marker)
#include "simu5g/stack/rlc/um/NrRlcUmDataPdu.h"             // NrRlcUmDataPdu (NR UM, SO segmentation)
#include "simu5g/stack/rlc/am/packet/NrRlcAmDataPdu.h"      // NrRlcAmDataPdu (NR AM, poll bit + SO)
#include "simu5g/stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"  // NrRlcAmStatusPdu (NR AM STATUS)
#include "simu5g/stack/rlc/LteRlcDefs_m.h"                  // LteAmType { DATA=0, ACK=1 }

namespace inet {
namespace protocoltest {
namespace rlctest {

using namespace simu5g;

// ---------------------------------------------------------------------------
// low-level readers: peek the front chunk, tolerating any packet that is not an
// RLC PDU of the requested kind (returns nullptr rather than throwing).
// ---------------------------------------------------------------------------

template<typename T>
inline Ptr<const T> front(const Packet *pkt)
{
    if (pkt == nullptr)
        return nullptr;
    try {
        if (pkt->hasAtFront<T>())
            return pkt->peekAtFront<T>();
    }
    catch (const std::exception&) { /* not this chunk type -> non-match */ }
    return nullptr;
}

inline const Packet *pkt(const MatchContext& c) { return c.event.packet; }

// A "sentPacketToLowerLayer" emission on the mux is sometimes a MAC new-data
// *notification* (a marker carrying LteRlcNewDataTag), not a real PDU. Every data
// predicate below also checks the chunk type, which already excludes notifications;
// this is exposed for explicit .never(...) filtering when needed.
inline bool isNewDataNotification(const MatchContext& c)
{
    return pkt(c) != nullptr && pkt(c)->findTag<LteRlcNewDataTag>() != nullptr;
}

// ===========================================================================
// TM -- transparent mode (LTE TS 36.322 5.1.1 / NR TS 38.322 5.2.1)
// TM adds no RLC header: the on-wire chunk is the upper-layer (PDCP) PDU, so the
// packet carries NO RLC data/AM chunk at the front. "TM pass-through" == a PDU on
// the RLC->MAC boundary that is not any RLC chunk kind.
// ===========================================================================

inline bool hasAnyRlcHeader(const MatchContext& c)
{
    return front<LteRlcUmDataPdu>(pkt(c)) != nullptr
        || front<NrRlcUmDataPdu>(pkt(c)) != nullptr
        || front<LteRlcAmPdu>(pkt(c)) != nullptr
        || front<NrRlcAmDataPdu>(pkt(c)) != nullptr
        || front<NrRlcAmStatusPdu>(pkt(c)) != nullptr;
}

inline bool isTmPassThrough(const MatchContext& c)
{
    return pkt(c) != nullptr && !hasAnyRlcHeader(c) && !isNewDataNotification(c);
}

// ===========================================================================
// UM -- LTE (LteRlcUmDataPdu; msg-generated, fields reflectable)
// ===========================================================================

inline bool isLteUm(const MatchContext& c) { return front<LteRlcUmDataPdu>(pkt(c)) != nullptr; }

inline bool lteUmSnIs(const MatchContext& c, unsigned int sn)
{
    auto h = front<LteRlcUmDataPdu>(pkt(c));
    return h != nullptr && h->getPduSequenceNumber() == sn;
}

// FramingInfo (TS 36.322 6.2.2.6): first/last chunk of the PDU is a fragment.
inline bool lteUmStartsWithFragment(const MatchContext& c)
{
    auto h = front<LteRlcUmDataPdu>(pkt(c));
    return h != nullptr && h->getFramingInfo().firstIsFragment;
}
inline bool lteUmEndsWithFragment(const MatchContext& c)
{
    auto h = front<LteRlcUmDataPdu>(pkt(c));
    return h != nullptr && h->getFramingInfo().lastIsFragment;
}
inline bool lteUmIsSegment(const MatchContext& c)
{
    return lteUmStartsWithFragment(c) || lteUmEndsWithFragment(c);
}

// ===========================================================================
// UM -- NR (NrRlcUmDataPdu; hand-written, read via getters)
// NR UM segments an SDU into byte ranges [startOffset, endOffset] carrying SN
// snoMainPacket (TS 38.322 5.2.2 / 6.2.3.4-6.2.3.5).
// ===========================================================================

inline bool isNrUm(const MatchContext& c) { return front<NrRlcUmDataPdu>(pkt(c)) != nullptr; }

inline bool nrUmSnIs(const MatchContext& c, unsigned int sn)
{
    auto h = front<NrRlcUmDataPdu>(pkt(c));
    return h != nullptr && h->getSnoMainPacket() == sn;
}

// A genuine segment: covers less than the whole SDU (startOffset>0 or endOffset<len-1).
inline bool nrUmIsSegment(const MatchContext& c)
{
    auto h = front<NrRlcUmDataPdu>(pkt(c));
    if (h == nullptr)
        return false;
    return h->getStartOffset() != 0 || (h->getEndOffset() + 1) < h->getLengthMainPacket();
}

// The first segment of an SDU (startOffset == 0 but not the whole SDU).
inline bool nrUmIsFirstSegment(const MatchContext& c)
{
    auto h = front<NrRlcUmDataPdu>(pkt(c));
    return h != nullptr && h->getStartOffset() == 0 && (h->getEndOffset() + 1) < h->getLengthMainPacket();
}

// ===========================================================================
// AM -- LTE (LteRlcAmPdu; one class for DATA and STATUS, discriminated by amType)
// ===========================================================================

inline bool isLteAm(const MatchContext& c) { return front<LteRlcAmPdu>(pkt(c)) != nullptr; }

inline bool isLteAmData(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == DATA;
}
inline bool isLteAmStatus(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == ACK;
}
inline bool lteAmSnIs(const MatchContext& c, unsigned int sn)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getSnoMainPacket() == sn;
}
// LTE AM per-PDU SN = snoFragment (VT(S)), the value that increments per AMD PDU.
inline bool lteAmFragSnIs(const MatchContext& c, unsigned int sn)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == DATA && h->getSnoFragment() == sn;
}
// Fragment position within the SDU (isWhole/isFirst/isLast helpers on LteRlcAmPdu).
inline bool lteAmIsWholeSdu(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == DATA && h->isWhole();
}
inline bool lteAmIsFirstFrag(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == DATA && !h->isWhole() && h->isFirst();
}
inline bool lteAmIsLastFrag(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == DATA && !h->isWhole() && h->isLast();
}
// STATUS ACK_SN (TS 36.322 6.2.2.14) -- everything below ACK_SN is acknowledged.
inline bool lteAmStatusAckSnIs(const MatchContext& c, uint32_t ackSn)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == ACK && h->getData().ackSn == ackSn;
}
inline bool lteAmStatusHasNack(const MatchContext& c)
{
    auto h = front<LteRlcAmPdu>(pkt(c));
    return h != nullptr && h->getAmType() == ACK && !h->getData().nacks.empty();
}
// LTE AM has NO poll (P) field -- this is a modeling gap (TS 36.322 5.2.2). Always
// false; a faithful poll-bit test therefore fails => NOT-MODELED / EXPECTEDFAIL.
inline bool lteAmHasPollBit(const MatchContext&) { return false; }

// ===========================================================================
// AM -- NR data (NrRlcAmDataPdu; poll bit + SO byte range)
// ===========================================================================

inline bool isNrAmData(const MatchContext& c) { return front<NrRlcAmDataPdu>(pkt(c)) != nullptr; }

// Poll bit P (TS 38.322 5.3.3 / 6.2.3.7).
inline bool nrAmPollBitSet(const MatchContext& c)
{
    auto h = front<NrRlcAmDataPdu>(pkt(c));
    return h != nullptr && h->getPollStatus();
}
inline bool nrAmSnIs(const MatchContext& c, unsigned int sn)
{
    auto h = front<NrRlcAmDataPdu>(pkt(c));
    return h != nullptr && h->getSnoMainPacket() == sn;
}
inline bool nrAmIsSegment(const MatchContext& c)
{
    auto h = front<NrRlcAmDataPdu>(pkt(c));
    if (h == nullptr)
        return false;
    return h->getStartOffset() != 0 || (h->getEndOffset() + 1) < h->getLengthMainPacket();
}

// ===========================================================================
// AM -- NR STATUS (NrRlcAmStatusPdu; msg-generated wrapper, StatusPduData payload)
// ===========================================================================

inline bool isNrAmStatus(const MatchContext& c) { return front<NrRlcAmStatusPdu>(pkt(c)) != nullptr; }

inline bool nrAmStatusAckSnIs(const MatchContext& c, uint32_t ackSn)
{
    auto h = front<NrRlcAmStatusPdu>(pkt(c));
    return h != nullptr && h->getData().ackSn == ackSn;
}
inline bool nrAmStatusAckSnAtLeast(const MatchContext& c, uint32_t ackSn)
{
    auto h = front<NrRlcAmStatusPdu>(pkt(c));
    return h != nullptr && h->getData().ackSn >= ackSn;
}
inline bool nrAmStatusHasNack(const MatchContext& c)
{
    auto h = front<NrRlcAmStatusPdu>(pkt(c));
    return h != nullptr && !h->getData().nacks.empty();
}
inline bool nrAmStatusNacksSn(const MatchContext& c, uint32_t sn)
{
    auto h = front<NrRlcAmStatusPdu>(pkt(c));
    if (h == nullptr)
        return false;
    for (const auto& n : h->getData().nacks)
        if (n.sn == sn)
            return true;
    return false;
}

} // namespace rlctest
} // namespace protocoltest
} // namespace inet

#endif
