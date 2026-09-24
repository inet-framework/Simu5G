//
//                  Simu5G
//
// Copyright (C) 2026 Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/common/L3Utils.h"

#include <inet/networklayer/ipv4/Ipv4Header_m.h>

namespace simu5g {

using namespace inet;

Ptr<const IpHeaderFieldsTag> attachIpHeaderFields(Packet *pkt)
{
    const auto& ipv4Header = pkt->peekAtFront<Ipv4Header>();
    auto tag = pkt->addTagIfAbsent<IpHeaderFieldsTag>();
    tag->setSrcAddress(ipv4Header->getSrcAddress());
    tag->setDestAddress(ipv4Header->getDestAddress());
    tag->setTos(ipv4Header->getTypeOfService());
    return tag;
}

} // namespace simu5g
