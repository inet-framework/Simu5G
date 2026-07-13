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

#include "simu5g/stack/sidelink/common/SlBinder.h"

namespace simu5g {

using namespace inet;

Define_Module(SlHandoverPacketHolderUe);

void SlHandoverPacketHolderUe::initialize(int stage)
{
    HandoverPacketHolderUe::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL)
        slBinder_ = SlBinder::getInstance();
}

bool SlHandoverPacketHolderUe::isDeliverable(Packet *datagram)
{
    auto ipHeader = datagram->peekAtFront<Ipv4Header>();
    if (slBinder_->getDstL2IdForMulticastAddress(ipHeader->getDestAddress()) != SL_L2ID_NONE) {
        EV << "SlHandoverPacketHolderUe: PC5-destined packet is deliverable without a serving node" << endl;
        return true;
    }
    return HandoverPacketHolderUe::isDeliverable(datagram);
}

} // namespace simu5g
