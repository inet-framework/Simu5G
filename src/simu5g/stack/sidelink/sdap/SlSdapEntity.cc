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

#include "simu5g/stack/sidelink/sdap/SlSdapEntity.h"

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/QfiTag_m.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(SlSdapEntity);

void SlSdapEntity::initialize()
{
    isTx_ = (par("role").stdstringValue() == "tx");
    peerKey_ = (uint32_t)par("peerKey").intValue();
    slRrc_ = check_and_cast<SlRrc *>(getModuleByPath(par("slRrcModule").stringValue()));
    slBinder_ = SlBinder::getInstance();
}

void SlSdapEntity::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);
    if (isTx_)
        handleTxPacket(pkt);
    else
        handleRxPacket(pkt);
    send(pkt, "out");
}

std::vector<const SlrbConfigEntry *> SlSdapEntity::getSlrbCandidates() const
{
    std::vector<const SlrbConfigEntry *> candidates;

    // unicast links: the peer's L2 ID (TX key) or node id (RX key) resolves
    // to a link in the registry; its per-link SLRBs are the slice
    MacNodeId peerNodeId = isTx_ ? slBinder_->getNodeIdForL2Id((SlL2Id)peerKey_) : MacNodeId(peerKey_);
    const SlUnicastLink *link = (peerNodeId != NODEID_NONE) ? slRrc_->findLink(peerNodeId) : nullptr;
    if (link != nullptr) {
        for (const auto& e : link->slrbs)
            candidates.push_back(&e);
        return candidates;
    }

    // broadcast/groupcast: the preconfig entries of this destination (TX);
    // on RX the slice is every static entry -- the drbId/pfi lookups below
    // stay unambiguous because static DRB ids are unique per UE (D3 check)
    for (const auto& e : slRrc_->getPreconfig().slrbConfig)
        if (!isTx_ || e.dstL2Id == (SlL2Id)peerKey_)
            candidates.push_back(&e);
    return candidates;
}

void SlSdapEntity::handleTxPacket(Packet *pkt)
{
    int pfi = pkt->hasTag<QfiReq>() ? (int)pkt->getTag<QfiReq>()->getQfi() : 0;

    // entry with a matching PFI, else the default entry, else the first
    auto candidates = getSlrbCandidates();
    if (candidates.empty())
        throw cRuntimeError("SlSdapEntity: no SLRB candidates for peer key %u", peerKey_);
    const SlrbConfigEntry *chosen = nullptr;
    const SlrbConfigEntry *def = nullptr;
    for (const auto *e : candidates) {
        if (e->pfi == pfi) {
            chosen = e;
            break;
        }
        if (e->isDefault)
            def = e;
    }
    if (chosen == nullptr)
        chosen = (def != nullptr) ? def : candidates.front();

    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    lteInfo->setDrbId(chosen->drbId);

    EV << "SlSdapEntity::handleTxPacket - PFI " << pfi << " -> SLRB drb " << num(chosen->drbId)
       << " (rlc " << chosen->rlcType << ", pqi " << chosen->pqi << ")" << endl;
}

void SlSdapEntity::handleRxPacket(Packet *pkt)
{
    // reverse lookup by the delivering DRB; stamps the flow identity upward
    // (future home of reflective QoS / per-flow state)
    auto lteInfo = pkt->getTag<FlowControlInfo>();
    for (const auto *e : getSlrbCandidates()) {
        if (e->drbId == lteInfo->getDrbId()) {
            pkt->addTagIfAbsent<QfiInd>()->setQfi(Qfi(e->pfi));
            EV << "SlSdapEntity::handleRxPacket - drb " << num(e->drbId)
               << " -> QfiInd " << e->pfi << endl;
            return;
        }
    }
    EV << "SlSdapEntity::handleRxPacket - no SLRB entry for drb " << num(lteInfo->getDrbId())
       << ", no QfiInd stamped" << endl;
}

} // namespace simu5g
