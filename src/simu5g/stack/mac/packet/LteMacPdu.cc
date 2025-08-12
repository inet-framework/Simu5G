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

#include "simu5g/stack/mac/packet/LteMacPdu.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/common/packet/chunk/BytesChunk.h"

using namespace inet;

namespace simu5g {

Register_Class(LteMacPdu);

std::string LteMacPdu::str() const
{
    std::stringstream ss;
    std::string s;
    ss << (std::string) (getName()) << " containing " << getSduCount()
            << " SDUs and " << ce_arraysize << " CEs" << " with size "
            << getByteLength();
    s = ss.str();
    return s;
}

void LteMacPdu::forEachChild(cVisitor *v)
{
    FieldsChunk::forEachChild(v);
    if (subPdus != nullptr) {
        v->visit(subPdus);
    }
}

void LteMacPdu::pushSdu(Packet *pkt, LogicalCid lcid)
{
    B sduLength = pkt->getDataLength();

    // Create NrMacSubheader
    auto subheader = makeShared<NrMacSubheader>();
    subheader->setLcid(num(lcid));
    subheader->setSduLength(sduLength.get());
    subheader->setFBit(sduLength.get() > 255);  // Use 16-bit length if > 255 bytes

    // Adjust subheader length based on F bit
    if (subheader->getFBit()) {
        subheader->setChunkLength(B(3)); // LCID(1) + F(1) + Length(2) = 3 bytes
    } else {
        subheader->setChunkLength(B(2)); // LCID(1) + F(1) + Length(1) = 2 bytes
    }

    // Mark subheader as immutable before inserting
    subheader->markImmutable();

    // Extract packet content as chunk
    auto payloadChunk = pkt->peekDataAt(B(0), sduLength);

    // Create composed subPDU (header + payload)
    auto subPduChunk = makeShared<SequenceChunk>();
    subPduChunk->insertAtBack(subheader);
    subPduChunk->insertAtBack(payloadChunk);

    // Initialize or update the subPdus SequenceChunk
    if (subPdus == nullptr) {
        subPdus = new SequenceChunk();
    }

    // Add the new subPDU
    subPdus->insertAtBack(subPduChunk);

    // Update length tracking
    macPduLength_ += pkt->getByteLength();

    // Update the chunk length to include all subPDUs
    setChunkLength(B(macPduLength_ + getHeaderLength()));

    // Clean up the original packet
    delete pkt;
}

Packet* LteMacPdu::popSdu(LogicalCid& lcid)
{
    if (!hasSdu()) {
        throw cRuntimeError("LteMacPdu::popSdu: no SDUs available");
    }

    // Parse the first subPDU using iterator
    Chunk::Iterator iterator(true, b(0), 0);

    // Read NrMacSubheader
    auto subheader = subPdus->peek<NrMacSubheader>(iterator);
    subPdus->moveIterator(iterator, subheader->getChunkLength());

    // Read payload
    B payloadLength = B(subheader->getSduLength());
    auto payloadChunk = subPdus->peek(iterator, payloadLength);

    // Create new packet with the payload
    auto packet = new Packet("SDU");
    packet->insertAtBack(payloadChunk);

    lcid = LogicalCid(subheader->getLcid());

    // Update subPdus by removing the first subPDU
    B totalSubPduLength = subheader->getChunkLength() + payloadLength;
    if (totalSubPduLength < subPdus->getChunkLength()) {
        // There are more subPDUs, create new SequenceChunk with remaining content
        auto remainingChunk = subPdus->peek(Chunk::Iterator(true, totalSubPduLength, -1),
                                           subPdus->getChunkLength() - totalSubPduLength);
        delete subPdus;
        subPdus = new SequenceChunk();
        subPdus->insertAtBack(remainingChunk);
    } else {
        // This was the last subPDU, clear subPdus
        delete subPdus;
        subPdus = nullptr;
    }

    // Update length tracking
    macPduLength_ -= packet->getByteLength();
    setChunkLength(B(macPduLength_ + getHeaderLength()));

    return packet;
}

bool LteMacPdu::hasSdu() const
{
    return subPdus != nullptr && subPdus->getChunkLength() > B(0);
}

// Returns the subheader of the k-th subPDU, and the offset of its payload
static Ptr<const NrMacSubheader> findSubPdu(const SequenceChunk *subPdus, size_t k, b& payloadOffset)
{
    b currentOffset = b(0);
    size_t currentIndex = 0;

    // Iterate through subPDUs to find the k-th one
    while (subPdus != nullptr && currentOffset < subPdus->getChunkLength()) {
        // Read NrMacSubheader
        Chunk::Iterator headerIterator(true, currentOffset, -1);
        auto subheader = subPdus->peek<NrMacSubheader>(headerIterator);
        currentOffset += subheader->getChunkLength();

        if (currentIndex == k) {
            payloadOffset = currentOffset;
            return subheader;
        }

        // Skip payload
        currentOffset += B(subheader->getSduLength());
        currentIndex++;
    }

    throw cRuntimeError("LteMacPdu: SDU index %zu out of range", k);
}

Packet LteMacPdu::getSdu(size_t k) const
{
    b payloadOffset;
    auto subheader = findSubPdu(subPdus, k, payloadOffset);

    // Create new packet with the payload
    B payloadLength = B(subheader->getSduLength());
    auto payloadChunk = subPdus->peek(Chunk::Iterator(true, payloadOffset, -1), payloadLength);
    Packet packet("SDU");
    packet.insertAtBack(payloadChunk);
    return packet;
}

LogicalCid LteMacPdu::getLcid(size_t k) const
{
    b payloadOffset;
    return LogicalCid(findSubPdu(subPdus, k, payloadOffset)->getLcid());
}

size_t LteMacPdu::getSduCount() const
{
    if (!hasSdu()) {
        return 0;
    }

    b currentOffset = b(0);
    size_t count = 0;

    // Count subPDUs
    while (currentOffset < subPdus->getChunkLength()) {
        // Read NrMacSubheader
        Chunk::Iterator headerIterator(true, currentOffset, -1);
        auto subheader = subPdus->peek<NrMacSubheader>(headerIterator);
        currentOffset += subheader->getChunkLength();

        // Skip payload
        B payloadLength = B(subheader->getSduLength());
        currentOffset += payloadLength;
        count++;
    }

    return count;
}

} //namespace
