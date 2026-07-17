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

#include <inet/common/socket/SocketTag_m.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/common/QfiTag_m.h"
#include "simu5g/stack/rrc/BearerManagement.h"
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
        bearerManagement_ = check_and_cast<BearerManagement *>(getModuleByPath(par("bearerManagementModule").stringValue()));
        pc5UnicastEnabled_ = par("pc5UnicastEnabled");
        hasSlSdap_ = par("hasSlSdap");
        useDscpAsPfiFallback_ = par("useDscpAsPfiFallback");
    }
}

void SlIp2Nic::handleMessage(cMessage *msg)
{
    cGate *arrival = msg->getArrivalGate();
    if (arrival->isName("sdapTxIn")) {
        onSdapTxReturn(check_and_cast<Packet *>(msg));
        return;
    }
    if (arrival->isName("sdapRxIn")) {
        onSdapRxReturn(check_and_cast<Packet *>(msg));
        return;
    }
    if (hasSlSdap_ && arrival->isName("stackIn")) {
        // received SL packets take the RX side chain (D20) with their tags
        // intact; the base tag strip continues on return
        auto pkt = check_and_cast<Packet *>(msg);
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr && lteInfo->getDirection() == SL) {
            routeViaSdapRx(pkt, (uint32_t)num(lteInfo->getSourceId()));
            return;
        }
    }
    Ip2Nic::handleMessage(msg);
}

void SlIp2Nic::toStackUe(Packet *pkt)
{
    EV << "SlIp2Nic::toStackUe - message from IP layer: send to stack: " << pkt->str() << std::endl;
    auto ipHeader = pkt->peekAtFront<Ipv4Header>();
    analyzePacket(pkt, ipHeader->getSrcAddress(), ipHeader->getDestAddress(), ipHeader->getTypeOfService());

    // classified SL packets take the SDAP side chain (D20): the entity maps
    // the resolved PFI to the serving SLRB and returns the packet
    if (hasSlSdap_) {
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr && lteInfo->getDirection() == SL) {
            int pfi = resolvePfi(pkt);
            pkt->addTagIfAbsent<QfiReq>()->setQfi(Qfi(pfi));
            routeViaSdapTx(pkt, lteInfo->getSlDstL2Id());
            return;
        }
    }

    send(pkt, stackGateOut_);
}

void SlIp2Nic::analyzePacket(Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    SlL2Id dstL2Id = slBinder_->getDstL2IdForMulticastAddress(destAddr);
    if (dstL2Id == SL_L2ID_NONE) {
        // PC5 unicast classification (D16): a unicast destination that is a
        // registered SL-capable UE goes over the sidelink (static rule; the
        // Uu/PC5 path-selection policy hook is SL-3)
        if (pc5UnicastEnabled_) {
            MacNodeId peerId = slBinder_->getPc5UnicastPeer(binder_.get(), destAddr, nrNodeId_);
            if (peerId != NODEID_NONE) {
                analyzeUnicastPc5Packet(pkt, peerId);
                return;
            }
        }
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
    // on first use (the PDCP entity registry is authoritative). With SL-SDAP
    // the final SLRB is only known after the PFI mapping: establishment
    // moves to onSdapTxReturn.
    if (!hasSlSdap_ && pdcpMux_->lookupTxEntity(DrbKey(dstPid, slrb->drbId)) == nullptr)
        slBinder_->establishSlConnection(lteInfo.get());
}

void SlIp2Nic::analyzeUnicastPc5Packet(Packet *pkt, MacNodeId peerId)
{
    // establish (or fetch) the PC5 unicast link; genie: synchronous, so the
    // triggering packet needs no holding. All link SLRBs come up with the
    // link, so the SDAP mapping (D20) never establishes anything later.
    const SlUnicastLink& link = slRrc_->establishLink(peerId);

    // without SL-SDAP every packet rides the link's default SLRB; with it,
    // the drb/rlcType stamped here are provisional until the PFI mapping
    const SlrbConfigEntry& slrb = link.findSlrbForPfi(0);

    EV << "SlIp2Nic::analyzeUnicastPc5Packet - PC5 unicast packet to peer " << peerId
       << " (dstL2Id=" << slrb.dstL2Id << ", drb " << num(slrb.drbId) << ")" << endl;

    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    lteInfo->setDirection(SL);
    lteInfo->setSourceId(nrNodeId_);
    lteInfo->setDestId(peerId);
    lteInfo->setSlSrcL2Id(slRrc_->getSrcL2Id());
    lteInfo->setSlDstL2Id(slrb.dstL2Id);
    lteInfo->setSlCastType(SL_UNICAST);
    lteInfo->setDrbId(slrb.drbId);
    lteInfo->setRlcType(slrb.rlcType);
    lteInfo->setTraffic(BACKGROUND);
}

int SlIp2Nic::resolvePfi(Packet *pkt)
{
    // resolution chain (D20): explicit QfiReq tag -> DSCP fallback -> default
    if (pkt->hasTag<QfiReq>())
        return (int)pkt->getTag<QfiReq>()->getQfi();
    if (useDscpAsPfiFallback_) {
        auto ipHeader = pkt->peekAtFront<Ipv4Header>();
        uint8_t tos = (uint8_t)ipHeader->getTypeOfService();
        if (tos > 0)
            return tos >> 2;
    }
    return 0;
}

void SlIp2Nic::routeViaSdapTx(Packet *pkt, uint32_t dstL2Id)
{
    auto it = sdapTxGates_.find(dstL2Id);
    if (it == sdapTxGates_.end())
        it = sdapTxGates_.emplace(dstL2Id, bearerManagement_->createSlSdapEntity(true, dstL2Id, this)).first;
    send(pkt, "sdapTxOut", it->second);
}

void SlIp2Nic::routeViaSdapRx(Packet *pkt, uint32_t srcPid)
{
    auto it = sdapRxGates_.find(srcPid);
    if (it == sdapRxGates_.end())
        it = sdapRxGates_.emplace(srcPid, bearerManagement_->createSlSdapEntity(false, srcPid, this)).first;
    send(pkt, "sdapRxOut", it->second);
}

void SlIp2Nic::onSdapTxReturn(Packet *pkt)
{
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    // establish the (now final) SLRB for broadcast/groupcast destinations;
    // unicast link SLRBs were all established with the link
    if ((SlCastType)lteInfo->getSlCastType() != SL_UNICAST
        && pdcpMux_->lookupTxEntity(DrbKey(lteInfo->getDestId(), lteInfo->getDrbId())) == nullptr)
        slBinder_->establishSlConnection(lteInfo.get());

    send(pkt, stackGateOut_);
}

void SlIp2Nic::onSdapRxReturn(Packet *pkt)
{
    // continue the base stackIn processing: strip the stack tags, hand to IP
    pkt->removeTagIfPresent<SocketInd>();
    removeAllSimu5GTags(pkt);
    toIpUe(pkt);
}

} // namespace simu5g
