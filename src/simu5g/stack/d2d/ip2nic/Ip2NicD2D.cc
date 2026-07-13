//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//
#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include "simu5g/stack/d2d/ip2nic/Ip2NicD2D.h"
#include "simu5g/stack/d2d/binder/D2dBinder.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(Ip2NicD2D);

void Ip2NicD2D::initialize(int stage)
{
    Ip2Nic::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS)
        d2dBinder_ = D2dBinder::getInstance(this);
}

MacNodeId Ip2NicD2D::getNextHopNodeId(const Ipv4Address& destAddr, bool useNR, MacNodeId sourceId)
{
    if (nodeType_ == NODEB)
        return Ip2Nic::getNextHopNodeId(destAddr, useNR, sourceId);  // eNB next-hop is D2D-agnostic

    // D2D-capable UE: check if direct D2D communication is possible
    MacNodeId destId = binder_->getMacNodeId(destAddr);
    MacNodeId srcId = isNR_ ? (useNR ? nrNodeId_ : nodeId_) : nodeId_;

    // check whether the destination is inside the LTE network and D2D is active
    if (destId == NODEID_NONE ||
        !(d2dBinder_->getD2DCapability(srcId, destId) && d2dBinder_->getD2DMode(srcId, destId) == DM)) {
        // packet is destined to the eNB; UE is subject to handovers: master may change
        return binder_->getServingNodeOrSelf(sourceId);
    }

    return destId;
}

void Ip2NicD2D::analyzePacket(inet::Packet *pkt, Ipv4Address srcAddr, Ipv4Address destAddr, uint16_t typeOfService)
{
    // --- Common preamble ---
    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();

    // Traffic category, RLC type (skipped when SDAP handles DRB/RLC assignment)
    if (!hasSdap_) {
        LteTrafficClass trafficCategory = getTrafficCategory(pkt);
        LteRlcType rlcType = getRlcType(trafficCategory);
        lteInfo->setTraffic(trafficCategory);
        lteInfo->setRlcType(rlcType);
    }

    // direction of transmitted packets depends on node type
    Direction dir = (nodeType_ == UE) ? UL : DL;
    lteInfo->setDirection(dir);

    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();
    bool isEnb = (dir == DL);

    // --- D2D-capable subclasses (LtePdcpEnbD2D, LtePdcpUeD2D, NrPdcpEnb, NrPdcpUe) ---

    // For NrPdcpUe, the effective local node ID depends on useNR flag
    MacNodeId localNodeId = (isNR_ && !isEnb) ? (useNR ? nrNodeId_ : nodeId_) : nodeId_;

    // EV log (all D2D subclasses except NrPdcpUe)
    if (isEnb || !isNR_)
        EV << "Received packet from data port, src= " << srcAddr << " dest=" << destAddr << " ToS=" << typeOfService << endl;

    if (isEnb) {
        // ENB: set D2D peer IDs to none
        lteInfo->setD2dTxPeerId(NODEID_NONE); // nem kell
        lteInfo->setD2dRxPeerId(NODEID_NONE);
    }
    else {
        // UE: D2D multicast/unicast handling (unified for NrPdcpUe and LtePdcpUeD2D)
        if (isNR_)
            lteInfo->setSourceId(localNodeId);

        if (destAddr.isMulticast()) {
            d2dBinder_->addD2DMulticastTransmitter(localNodeId);
            lteInfo->setDirection(D2D_MULTI);
            MacNodeId groupId = binder_->getOrAssignDestIdForMulticastAddress(destAddr);
            lteInfo->setMulticastGroupId(groupId);
        }
        else {
            MacNodeId destId = binder_->getMacNodeId(destAddr);
            if (destId != NODEID_NONE) { // the destination is a UE within the LTE network
                if (d2dBinder_->checkD2DCapability(localNodeId, destId)) {
                    // this way, we record the ID of the endpoints even if the connection is currently in IM
                    // this is useful for mode switching
                    lteInfo->setD2dTxPeerId(localNodeId);
                    lteInfo->setD2dRxPeerId(destId);
                }
                else {
                    lteInfo->setD2dTxPeerId(NODEID_NONE);
                    lteInfo->setD2dRxPeerId(NODEID_NONE);
                }

                // set actual flow direction (D2D/UL) based on the current mode (DM/IM) of this peering
                if (d2dBinder_->getD2DCapability(localNodeId, destId) && d2dBinder_->getD2DMode(localNodeId, destId) == DM)
                    lteInfo->setDirection(D2D);
                else
                    lteInfo->setDirection(UL);
            }
            else { // the destination is outside the LTE network
                lteInfo->setDirection(UL);
                lteInfo->setD2dTxPeerId(NODEID_NONE);
                lteInfo->setD2dRxPeerId(NODEID_NONE);
            }
        }
    }

    // --- Source and Dest IDs ---
    if (isNR_) {
        // For PDCP entity dispatch, always use technology-neutral (LTE/master-leg) IDs.
        // The TechnologyReq::useNR flag carries the LTE-vs-NR routing decision separately;
        // NrTxPdcpEntity reads it in deliverPdcpPdu() to decide local RLC vs X2 forwarding.
        if (isEnb) {
            lteInfo->setSourceId(nodeId_);
            if (lteInfo->getMulticastGroupId() != NODEID_NONE)
                lteInfo->setDestId(nodeId_);
            else
                lteInfo->setDestId(getNextHopNodeId(destAddr, !dualConnectivityEnabled_ && useNR, nodeId_));
        }
        else {
            // UE: use LTE UE ID when DC is enabled (both legs share one PDCP entity),
            // NR UE ID when non-DC NR (entity was created with NR IDs)
            MacNodeId ueSourceId = (dualConnectivityEnabled_ ? nodeId_ : (useNR ? nrNodeId_ : nodeId_));
            lteInfo->setSourceId(ueSourceId);
            if (lteInfo->getMulticastGroupId() != NODEID_NONE)
                lteInfo->setDestId(nodeId_);
            else
                lteInfo->setDestId(getNextHopNodeId(destAddr, !dualConnectivityEnabled_ && useNR, ueSourceId));
        }
    }
    else {
        // LtePdcpEnbD2D / LtePdcpUeD2D
        lteInfo->setSourceId(nodeId_);
        if (!isEnb) // LtePdcpUeD2D: dead getNextHopNodeId call (result unused in original code)
            (void)getNextHopNodeId(destAddr, useNR, lteInfo->getSourceId());

        lteInfo->setSourceId(nodeId_);   // TODO CHANGE HERE!!! Must be the NR node ID if this is an NR connection
        if (lteInfo->getMulticastGroupId() != NODEID_NONE)  // destId is meaningless for multicast D2D
            lteInfo->setDestId(nodeId_);
        else
            lteInfo->setDestId(getNextHopNodeId(destAddr, false, lteInfo->getSourceId()));
    }

    // --- DRB ID assignment (skipped when SDAP handles it) ---
    if (!hasSdap_) {
        ConnectionKey key{srcAddr, destAddr, typeOfService, lteInfo->getDirection()};
        DrbId drbId = lookupOrAssignDrbId(key);
        lteInfo->setDrbId(drbId);

        // Establish unless the PDCP TX entity already exists (authoritative check)
        if (pdcpMux_->lookupTxEntity(DrbKey(lteInfo->getDestId(), drbId)) == nullptr)
            binder_->establishUnidirectionalDataConnection(lteInfo.get());

        // Debug logging (UE subclasses only)
        if (!isEnb) {
            if (isNR_) {
                EV << "NrPdcpUe : Assigned DRB ID: " << drbId << "\n";
                EV << "NrPdcpUe : Assigned Node ID: " << localNodeId << "\n";
            }
            else {
                EV << "LtePdcpUeD2D : Assigned DRB ID: " << drbId << "\n";
                EV << "LtePdcpUeD2D : Assigned Node ID: " << nodeId_ << "\n";
            }
        }
    }
}

} //namespace
