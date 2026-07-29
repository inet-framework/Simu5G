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

#include <climits>
#include "simu5g/stack/mac/buffer/LteMacQueue.h"
#include "simu5g/stack/rlc/packet/LteRlcPdu_m.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

LteMacQueue::LteMacQueue(int queueSize) :
    cPacketQueue("LteMacQueue"), queueSize_(queueSize)
{
}

LteMacQueue::LteMacQueue(const LteMacQueue& queue)
{
    operator=(queue);
}

LteMacQueue& LteMacQueue::operator=(const LteMacQueue& queue)
{
    cPacketQueue::operator=(queue);
    queueSize_ = queue.queueSize_;
    return *this;
}

LteMacQueue *LteMacQueue::dup() const
{
    return new LteMacQueue(*this);
}

// ENQUEUE
bool LteMacQueue::pushBack(cPacket *pkt)
{
    Packet *pktAux = check_and_cast<Packet *>(pkt);
    if (!isEnqueueablePacket(pktAux))
        return false; // packet queue full or we have discarded fragments for this main packet

    cPacketQueue::insert(pkt);
    return true;
}

bool LteMacQueue::pushFront(cPacket *pkt)
{
    Packet *pktAux = check_and_cast<Packet *>(pkt);
    if (!isEnqueueablePacket(pktAux))
        return false; // packet queue full or we have discarded fragments for this main packet

    cPacketQueue::insertBefore(cPacketQueue::front(), pkt);
    return true;
}

cPacket *LteMacQueue::popFront()
{
    return getQueueLength() > 0 ? cPacketQueue::pop() : nullptr;
}

cPacket *LteMacQueue::popBack()
{
    return getQueueLength() > 0 ? cPacketQueue::remove(cPacketQueue::back()) : nullptr;
}

simtime_t LteMacQueue::getHolTimestamp() const
{
    return getQueueLength() > 0 ? cPacketQueue::front()->getTimestamp() : 0;
}

int64_t LteMacQueue::getQueueOccupancy() const
{
    return cPacketQueue::getByteLength();
}

int64_t LteMacQueue::getQueueSize() const
{
    return queueSize_;
}

bool LteMacQueue::isEnqueueablePacket(Packet *pkt) {

    if (queueSize_ == 0) {
        // unlimited queue size -- nothing to check for
        return true;
    }

    // Every RLC mode sends PDUs only upon a MAC SDU request, sized to the grant,
    // so a packet is enqueueable whenever it fits. (The old LTE AM entity
    // pre-built fixed-size fragments and needed an all-fragments-will-fit check
    // here; that design is gone.)
    return pkt->getByteLength() + getByteLength() < queueSize_;
}

int LteMacQueue::getQueueLength() const
{
    return cPacketQueue::getLength();
}

std::ostream& operator<<(std::ostream& stream, const LteMacQueue *queue)
{
    stream << "LteMacQueue-> Length: " << queue->getQueueLength() <<
        " Occupancy: " << queue->getQueueOccupancy() <<
        " HolTimestamp: " << queue->getHolTimestamp() <<
        " Size: " << queue->getQueueSize();
    return stream;
}

} //namespace

