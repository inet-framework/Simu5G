//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "MacDrbMultiplexer.h"
#include "simu5g/common/RadioBearerTag_m.h"

namespace simu5g {

Define_Module(MacDrbMultiplexer);

void MacDrbMultiplexer::initialize()
{
}

void MacDrbMultiplexer::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *incomingGate = pkt->getArrivalGate();

    if (incomingGate == gate("macIn")) {
        // MAC → RLC: look up the correct RLC gate index from the learned mapping
        auto lteInfo = pkt->getTag<FlowControlInfo>();
        LogicalCid lcid = lteInfo->getLcid();
        auto it = lcidToDrb_.find(lcid);
        int drbIndex;
        if (it != lcidToDrb_.end()) {
            drbIndex = it->second;
        }
        else {
            // Fallback for packets arriving before the mapping is learned
            // (e.g., DL reception on UE before UL traffic is established)
            int numDrbs = gateSize("rlcOut");
            drbIndex = std::min((int)lcid, numDrbs - 1);
        }
        send(pkt, "rlcOut", drbIndex);
    }
    else {
        // RLC → MAC: learn the LCID-to-gate mapping from the incoming gate index
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr) {
            LogicalCid lcid = lteInfo->getLcid();
            int gateIndex = incomingGate->getIndex();
            lcidToDrb_[lcid] = gateIndex;
        }
        send(pkt, "macOut");
    }
}

} //namespace
