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

#include "simu5g/stack/sidelink/ip2nic/SlIp2Nic.h"

#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;

Define_Module(SlIp2Nic);

void SlIp2Nic::initialize(int stage)
{
    Ip2Nic::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        slBinder_ = SlBinder::getInstance();
        slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
    }
}

void SlIp2Nic::analyzePacket(Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    SlL2Id dstL2Id = slBinder_->getDstL2IdForMulticastAddress(destAddr);
    if (dstL2Id == SL_L2ID_NONE) {
        Ip2Nic::analyzePacket(pkt, srcAddr, destAddr, typeOfService);
        return;
    }

    EV << "SlIp2Nic::analyzePacket - PC5 packet, dest=" << destAddr << " -> dstL2Id=" << dstL2Id << endl;

    const SlrbConfigEntry *slrb = slRrc_->getPreconfig().findSlrbForDstL2Id(dstL2Id);
    if (slrb == nullptr)
        throw cRuntimeError("SlIp2Nic: destination L2 ID %u has no slrbConfig entry", dstL2Id);

    MacNodeId dstPid = slBinder_->getOrAssignGroupL2Pid(dstL2Id);

    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    lteInfo->setDirection(SL);
    lteInfo->setSourceId(nrNodeId_);
    lteInfo->setDestId(dstPid);
    lteInfo->setSlSrcL2Id(slRrc_->getSrcL2Id());
    lteInfo->setSlDstL2Id(dstL2Id);
    lteInfo->setSlCastType(slrb->castType);
    lteInfo->setDrbId(slrb->drbId);
    lteInfo->setRlcType(slrb->rlcType);
    lteInfo->setTraffic(BACKGROUND);

    // genie establishment of the SLRB chains at the sender and all receivers
    // on first use (the PDCP entity registry is authoritative)
    if (pdcpMux_->lookupTxEntity(DrbKey(dstPid, slrb->drbId)) == nullptr)
        slBinder_->establishSlConnection(lteInfo.get());
}

} // namespace simu5g
