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

#include "simu5g/stack/sidelink/ip2nic/SlHandoverPacketHolderUe.h"

#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/stack/sidelink/ip2nic/SlIp2Nic.h"

namespace simu5g {

using namespace inet;

Define_Module(SlHandoverPacketHolderUe);

void SlHandoverPacketHolderUe::initialize(int stage)
{
    HandoverPacketHolderUe::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        // G27: the path decision is SlIp2Nic's - one shared function (D33)
        slIp2Nic_ = check_and_cast<SlIp2Nic *>(getModuleByPath(par("slIp2NicModule").stringValue()));
    }
}

bool SlHandoverPacketHolderUe::isDeliverable(Packet *datagram)
{
    auto ipHeader = datagram->peekAtFront<Ipv4Header>();
    inet::Ipv4Address destAddr = ipHeader->getDestAddress();

    // G27: same shared decision as SlTechnologyDecision/SlIp2Nic (D33).
    // PATH_DENY packets are passed through as "deliverable" so they reach
    // the single drop point at SlTechnologyDecision instead of being held
    // forever here.
    switch (slIp2Nic_->decidePath(destAddr, ipHeader->getTypeOfService())) {
        case SlPathPolicy::PATH_PC5:
            EV << "SlHandoverPacketHolderUe: PC5-destined packet is deliverable without a serving node" << endl;
            return true;
        case SlPathPolicy::PATH_DENY:
            return true;
        case SlPathPolicy::PATH_UU:
            break;
    }
    return HandoverPacketHolderUe::isDeliverable(datagram);
}

} // namespace simu5g
