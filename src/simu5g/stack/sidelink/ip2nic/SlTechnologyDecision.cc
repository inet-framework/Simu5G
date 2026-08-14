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
#include "simu5g/stack/sidelink/ip2nic/SlIp2Nic.h"

namespace simu5g {

using namespace inet;

Define_Module(SlTechnologyDecision);

void SlTechnologyDecision::initialize(int stage)
{
    TechnologyDecision::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        // G27: the path decision is SlIp2Nic's - one shared function, so the
        // three classification seams cannot drift apart per packet (D33)
        slIp2Nic_ = check_and_cast<SlIp2Nic *>(getModuleByPath(par("slIp2NicModule").stringValue()));
        EV << "SlTechnologyDecision::initialize - PC5 classification active" << endl;
    }
}

void SlTechnologyDecision::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    auto ipHeader = pkt->peekAtFront<inet::Ipv4Header>();
    inet::Ipv4Address destAddr = ipHeader->getDestAddress();

    switch (slIp2Nic_->decidePath(destAddr, ipHeader->getTypeOfService())) {
        case SlPathPolicy::PATH_PC5:
            EV << "SlTechnologyDecision: PC5-destined packet (dest=" << destAddr
               << "), bypassing the serving-node check" << endl;
            pkt->addTagIfAbsent<TechnologyReq>()->setUseNR(true);
            send(pkt, lowerLayerOut_);
            return;
        case SlPathPolicy::PATH_DENY:
            EV_WARN << "SlTechnologyDecision: dropping unicast to " << destAddr
                    << ": pathSelectionPolicy=\"pc5Only\" and the destination is not an SL peer" << endl;
            delete pkt;
            return;
        case SlPathPolicy::PATH_UU:
            break;
    }

    TechnologyDecision::handleMessage(msg);
}

} // namespace simu5g
