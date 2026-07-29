//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.common.packet.Packet;
import simu5g.stack.rlc.LteRlcDefs;

cplusplus {{
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
}}

class size_t {
    @existingClass; // size_t
    @primitive;
    @fromString(string2uint64($));
    @toString(uint642string($));
    @defaultValue(0);
    @toValue((omnetpp::intval_t)($));
    @fromValue(omnetpp::checked_int_cast<size_t>($.intValue()));
}

namespace simu5g;

class LteRlcDataPdu extends inet::FieldsChunk
{
    chunkLength = inet::b(1);

    inet::Packet* sdu[] @sizeGetter(getNumSdu) @owned;
    size_t sduSize[];

    // Sequence number of the PDU
    unsigned int pduSequenceNumber = 0;

    // define the segmentation info for the PDU
    FramingInfo framingInfo;
}

cplusplus(LteRlcDataPdu) {{
  public:
    /**
     * pushSdu() gets ownership of the packet
     * and stores it inside the RLC SDU list
     * in back position
     *
     * @param pkt packet to store
     */
    virtual void pushSdu(inet::Packet *pkt);
    virtual void pushSdu(inet::Packet *pkt, int size);

    /**
     * popSdu() pops a packet from the front of
     * the SDU list and drops ownership before
     * returning it
     *
     * @return popped packet
     */
    virtual inet::Packet *popSdu(size_t& size);
}}

cplusplus(cc) {{

void LteRlcDataPdu::pushSdu(inet::Packet *pkt)
{
    pushSdu(pkt, pkt->getByteLength());
}

void LteRlcDataPdu::pushSdu(inet::Packet *pkt, int size)
{
    // Clear tags to make sure they are not "tunnelled" over to the receiver node and "accidentally"
    // used there -- with the exception of PdcpTrackingTag, as the RLC-UM RX entities (LteRlcUmRxEntity/NrRlcUmRxEntity) still refer to it.
    auto pdcpTag = pkt->removeTagIfPresent<PdcpTrackingTag>();
    pkt->clearTags();
    if (pdcpTag != nullptr)
        *pkt->addTag<PdcpTrackingTag>() = *pdcpTag;

    appendSdu(pkt);
    appendSduSize(size);
}

inet::Packet *LteRlcDataPdu::popSdu(size_t& size)
{
    auto pkt = removeSdu(0);
    eraseSdu(0);

    size = getSduSize(0);
    eraseSduSize(0);
    return pkt;
}

}}

//
// Lte RLC UM Data Pdu
//
// Define additional fields for UM PDU
//
class LteRlcUmDataPdu extends LteRlcDataPdu
{
    chunkLength = inet::B(RLC_HEADER_UM);
};

//
// LTE RLC AM data PDU: an AMD PDU or, when rf is set, an AMD PDU segment
// (3GPP TS 36.322 6.2.1.4 / 6.2.1.5).
//
// Concatenation is modeled exactly as in the UM data PDU (the inherited SDU
// list with per-SDU sizes plus the FI bits); an AMD PDU additionally carries
// the poll bit, and its sequence number (10-bit on the wire; unbounded in the
// model, as everywhere else) is the unit of ARQ.
//
// An AMD PDU segment -- a re-segmented retransmission built to fit a smaller
// grant -- carries the byte range [soStart, soEnd] of the original PDU's data
// field. As elsewhere in the model, the segment packet carries the original
// PDU's whole SDU list, and only chunkLength reflects the bytes on the air.
//
class LteRlcAmDataPdu extends LteRlcDataPdu
{
    chunkLength = inet::B(RLC_HEADER_AM);
    bool pollStatus = false;    // P bit (TS 36.322 5.2.2)
    bool rf = false;            // resegmentation flag: this is an AMD PDU segment
    unsigned int soStart = 0;   // segment offset of the first data byte carried
    unsigned int soEnd = 0;     // segment offset of the last data byte carried
    bool lsf = false;           // last segment flag
}

//
// LTE RLC AM STATUS PDU (TS 36.322 6.2.1.6): ACK_SN plus a NACK list, with
// byte ranges (SOstart/SOend) for partially received PDUs.
//
class LteRlcAmStatusPdu extends inet::FieldsChunk
{
    chunkLength = inet::B(2);
    StatusPduData data;
}
