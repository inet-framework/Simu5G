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

#include "simu5g/stack/sidelink/ip2nic/SlTechnologyDecision.h"

#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"

namespace simu5g {

using namespace inet;

Define_Module(SlTechnologyDecision);

void SlTechnologyDecision::initialize(int stage)
{
    TechnologyDecision::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        slBinder_ = SlBinder::getInstance();
        EV << "SlTechnologyDecision::initialize - PC5 classification active" << endl;
    }
}

void SlTechnologyDecision::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto ipHeader = pkt->peekAtFront<inet::Ipv4Header>();

    // PC5-destined packet? (destination registered as an SL multicast address)
    if (slBinder_->getDstL2IdForMulticastAddress(ipHeader->getDestAddress()) != SL_L2ID_NONE) {
        EV << "SlTechnologyDecision: PC5-destined packet (dest=" << ipHeader->getDestAddress()
           << "), bypassing the serving-node check" << endl;
        pkt->addTagIfAbsent<TechnologyReq>()->setUseNR(true);
        send(pkt, lowerLayerOut_);
        return;
    }

    TechnologyDecision::handleMessage(msg);
}

} // namespace simu5g
