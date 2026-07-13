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

#include "simu5g/stack/sidelink/common/SlBinder.h"

#include "simu5g/common/LteControlInfo.h"
#include "simu5g/stack/sidelink/rrc/SlRrc.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(SlBinder);

void SlBinder::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

SlBinder *SlBinder::getInstance()
{
    cModule *network = cSimulation::getActiveSimulation()->getSystemModule();
    cModule *m = network->getSubmodule("slBinder");
    if (m == nullptr) {
        // dynamically create the singleton on first use, so network NED files need no edits
        cModuleType *type = cModuleType::get("simu5g.stack.sidelink.common.SlBinder");
        m = type->create("slBinder", network);
        m->finalizeParameters();
        m->buildInside();
        m->scheduleStart(simTime());
        m->callInitialize();
    }
    return check_and_cast<SlBinder *>(m);
}

void SlBinder::registerUeL2Id(SlL2Id srcL2Id, MacNodeId nodeId)
{
    ASSERT(srcL2Id != SL_L2ID_NONE && nodeId != NODEID_NONE);
    auto it = l2IdToNodeId_.find(srcL2Id);
    if (it != l2IdToNodeId_.end() && it->second != nodeId)
        throw cRuntimeError("SlBinder: source L2 ID %u already registered by node %hu", srcL2Id, num(it->second));
    l2IdToNodeId_[srcL2Id] = nodeId;
    nodeIdToL2Id_[nodeId] = srcL2Id;
}

MacNodeId SlBinder::getNodeIdForL2Id(SlL2Id l2Id) const
{
    auto it = l2IdToNodeId_.find(l2Id);
    return it != l2IdToNodeId_.end() ? it->second : NODEID_NONE;
}

SlL2Id SlBinder::getL2IdForNodeId(MacNodeId nodeId) const
{
    auto it = nodeIdToL2Id_.find(nodeId);
    return it != nodeIdToL2Id_.end() ? it->second : SL_L2ID_NONE;
}

MacNodeId SlBinder::getOrAssignGroupL2Pid(SlL2Id dstL2Id)
{
    auto it = groupL2Pids_.find(dstL2Id);
    if (it != groupL2Pids_.end())
        return it->second;
    // allocate downward from SL_GROUP_PID_MAX (disjoint from Binder's ascending Uu allocator)
    if (groupPidCounter_ < MULTICAST_DEST_MIN_ID)
        throw cRuntimeError("SlBinder: out of sidelink group pseudo node-ids");
    MacNodeId pid = MacNodeId(groupPidCounter_--);
    groupL2Pids_[dstL2Id] = pid;
    groupPidToL2Id_[pid] = dstL2Id;
    return pid;
}

SlL2Id SlBinder::getL2IdForGroupPid(MacNodeId groupPid) const
{
    auto it = groupPidToL2Id_.find(groupPid);
    return it != groupPidToL2Id_.end() ? it->second : SL_L2ID_NONE;
}

void SlBinder::joinGroup(SlL2Id dstL2Id, MacNodeId member)
{
    groupMembers_[dstL2Id].insert(member);
}

const std::set<MacNodeId>& SlBinder::getGroupMembers(SlL2Id dstL2Id)
{
    return groupMembers_[dstL2Id];
}

bool SlBinder::isInGroup(SlL2Id dstL2Id, MacNodeId nodeId)
{
    auto it = groupMembers_.find(dstL2Id);
    return it != groupMembers_.end() && it->second.count(nodeId) > 0;
}

void SlBinder::registerMulticastAddress(inet::Ipv4Address addr, SlL2Id dstL2Id)
{
    auto it = multicastAddrToDstL2Id_.find(addr);
    if (it != multicastAddrToDstL2Id_.end() && it->second != dstL2Id)
        throw cRuntimeError("SlBinder: multicast address %s already mapped to dstL2Id %u", addr.str().c_str(), it->second);
    multicastAddrToDstL2Id_[addr] = dstL2Id;
}

SlL2Id SlBinder::getDstL2IdForMulticastAddress(inet::Ipv4Address addr) const
{
    auto it = multicastAddrToDstL2Id_.find(addr);
    return it != multicastAddrToDstL2Id_.end() ? it->second : SL_L2ID_NONE;
}

void SlBinder::registerSlPhy(MacNodeId nodeId, cModule *phyModule)
{
    slPhys_[nodeId] = phyModule;
}

void SlBinder::registerSlRrc(MacNodeId nodeId, SlRrc *slRrc)
{
    slRrcs_[nodeId] = slRrc;
}

void SlBinder::establishSlConnection(FlowControlInfo *lteInfo)
{
    MacNodeId senderId = lteInfo->getSourceId();
    ASSERT(lteInfo->getDirection() == SL);

    auto senderRrc = slRrcs_.find(senderId);
    if (senderRrc == slRrcs_.end())
        throw cRuntimeError("SlBinder: no SlRrc registered for sender node %hu", num(senderId));
    senderRrc->second->createSlOutgoingConnection(lteInfo);

    if ((SlCastType)lteInfo->getSlCastType() == SL_UNICAST) {
        MacNodeId peerId = lteInfo->getDestId();
        auto peerRrc = slRrcs_.find(peerId);
        if (peerRrc == slRrcs_.end())
            throw cRuntimeError("SlBinder: no SlRrc registered for peer node %hu", num(peerId));
        peerRrc->second->createSlIncomingConnection(lteInfo);
    }
    else {
        // broadcast/groupcast: fan out to every group member except the sender
        for (MacNodeId member : getGroupMembers((SlL2Id)lteInfo->getSlDstL2Id())) {
            if (member == senderId)
                continue;
            auto memberRrc = slRrcs_.find(member);
            if (memberRrc == slRrcs_.end())
                throw cRuntimeError("SlBinder: no SlRrc registered for group member node %hu", num(member));
            memberRrc->second->createSlIncomingConnection(lteInfo);
        }
    }
}

void SlBinder::registerSlCarrier(GHz carrierFrequency, unsigned int numerologyIndex, int subchannelSize, int numSubchannels)
{
    auto it = slCarriers_.find(carrierFrequency);
    if (it != slCarriers_.end()) {
        // all UEs must agree on the geometry of a shared pool
        const SlCarrierInfo& c = it->second;
        if (c.numerologyIndex != numerologyIndex || c.subchannelSize != subchannelSize || c.numSubchannels != numSubchannels)
            throw cRuntimeError("SlBinder: inconsistent SL carrier registration for %f GHz", carrierFrequency.get() / 1e9);
        return;
    }
    slCarriers_[carrierFrequency] = SlCarrierInfo{carrierFrequency, numerologyIndex, subchannelSize, numSubchannels};
}

const SlBinder::SlCarrierInfo *SlBinder::getSlCarrier(GHz carrierFrequency) const
{
    auto it = slCarriers_.find(carrierFrequency);
    return it != slCarriers_.end() ? &it->second : nullptr;
}

void SlBinder::recordSlTransmission(GHz carrierFrequency, SlotIndex slot, const SlTxRecord& record, SlotIndex pruneBefore)
{
    auto& slotMap = slTransmissionMap_[carrierFrequency];
    slotMap[slot].push_back(record);
    // lazy pruning on insert: drop slots older than the longest sensing window (no per-TTI rotation)
    slotMap.erase(slotMap.begin(), slotMap.lower_bound(pruneBefore));
}

const std::vector<SlBinder::SlTxRecord> *SlBinder::getSlTransmissions(GHz carrierFrequency, SlotIndex slot) const
{
    auto cit = slTransmissionMap_.find(carrierFrequency);
    if (cit == slTransmissionMap_.end())
        return nullptr;
    auto sit = cit->second.find(slot);
    return sit != cit->second.end() ? &sit->second : nullptr;
}

} // namespace simu5g
