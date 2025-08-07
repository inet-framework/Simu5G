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

#include "RlcToMacMultiplexer.h"
#include "simu5g/common/RadioBearerTag_m.h"

namespace simu5g {

Define_Module(RlcToMacMultiplexer);

void RlcToMacMultiplexer::initialize()
{
    numDrbs = par("numDrbs");
}

void RlcToMacMultiplexer::handleMessage(cMessage *msg)
{
    cPacket *pkt = check_and_cast<cPacket *>(msg);
    cGate *incoming = pkt->getArrivalGate();

    if (incoming == gate("macIn")) {
        // Packet coming from MAC --> Demux to correct RLC
        auto pktInet = check_and_cast<inet::Packet *>(pkt);
        auto lteInfo = pktInet->getTag<FlowControlInfo>();

        int drbIndex = lteInfo->getLcid();  // Assuming LCID == DRB index
        if (drbIndex >= numDrbs)
            throw cRuntimeError("RlcToMacMultiplexer: Invalid DRB index %d", drbIndex);

        // Add RadioBearerInd here (as MAC does not do it for now)
        auto drbTag = pktInet->addTagIfAbsent<RadioBearerInd>();
        drbTag->setDrbIndex(drbIndex);

        send(pkt, "rlcOut", drbIndex);
    }
    else {
        // Packet coming from RLC --> Forward to MAC
        send(pkt, "macOut");
    }
}

} //namespace
