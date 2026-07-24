#include "simu5g/stack/rlc/tm/RlcTmRxEntity.h"

namespace simu5g {

Define_Module(RlcTmRxEntity);

using namespace omnetpp;

void RlcTmRxEntity::initialize(int stage)
{
    // nothing to initialize
}

void RlcTmRxEntity::handleMessage(cMessage *msg)
{
    cGate *incoming = msg->getArrivalGate();
    if (incoming->isName("in")) {
        EV << "RlcTmRxEntity::handleMessage - forwarding packet " << msg->getName() << " to upper layer\n";
        send(msg, "out");
    }
    else {
        throw cRuntimeError("RlcTmRxEntity: unexpected message from gate %s", incoming->getFullName());
    }
}

} // namespace simu5g
