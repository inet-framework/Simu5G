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

#ifndef _SIDELINK_SLBINDER_H_
#define _SIDELINK_SLBINDER_H_

#include <map>
#include <set>
#include <vector>

#include <inet/common/geometry/common/Coord.h>
#include <inet/networklayer/contract/ipv4/Ipv4Address.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/common/SlUeRadioState.h"

namespace simu5g {

class Binder;
class SlRrc;
class SlGnbRrc;
class FlowControlInfo;

/**
 * Global sidelink registry (design decisions D4/D9), the sidelink analog of
 * Binder. A single instance exists per network; it is found or dynamically
 * created on first use (getInstance()), so network NED files need no edits.
 *
 * Holds:
 *  - the L2-ID <-> node registry: each UE's source L2 ID maps to its (NR) MAC
 *    node id; broadcast/groupcast destination L2 IDs map to allocated pseudo
 *    node ids ("L2Pids") drawn downward from SL_GROUP_PID_MAX, disjoint from
 *    the Uu multicast destination-id allocator (D4)
 *  - group membership (destination L2 ID -> member node ids)
 *  - the IP-multicast-address -> destination-L2-ID mapping used by the SL
 *    packet classification in ip2nic
 *  - the SL node registry (node id -> slPhy module) used for TX fan-out
 *  - the SL carrier registry: numerology/subchannel geometry of the SL
 *    carrier(s), deliberately NOT registered in Binder's Uu carrier registry
 *    so Uu MAC timing cannot be affected (gap G8)
 *  - the SL transmission map (D9): per-carrier, per-slot records of ongoing
 *    transmissions, written by slPhy at TX time and read by receivers for
 *    interference computation; pruned lazily on insert (no per-TTI rotation)
 */
class SlBinder : public omnetpp::cSimpleModule
{
  public:
    struct SlCarrierInfo {
        GHz carrierFrequency;
        unsigned int numerologyIndex = 0;
        int subchannelSize = 0;       // PRBs per subchannel
        int numSubchannels = 0;
    };

    struct SlTxRecord {
        MacNodeId txNodeId;
        int firstSubchannel = 0;
        int numSubchannels = 1;
        double txPower = 0;           // dBm
        inet::Coord txCoord;
    };

  protected:
    // L2-ID registry (D4)
    std::map<SlL2Id, MacNodeId> l2IdToNodeId_;
    std::map<MacNodeId, SlL2Id> nodeIdToL2Id_;

    // Group destination L2-ID -> allocated pseudo node id, and its reverse
    std::map<SlL2Id, MacNodeId> groupL2Pids_;
    std::map<MacNodeId, SlL2Id> groupPidToL2Id_;
    unsigned short groupPidCounter_ = SL_GROUP_PID_MAX;

    // Group membership: destination L2 ID -> member node ids
    std::map<SlL2Id, std::set<MacNodeId>> groupMembers_;

    // IP multicast address -> destination L2 ID
    std::map<inet::Ipv4Address, SlL2Id> multicastAddrToDstL2Id_;

    // SL-capable node registry: node id -> its slPhy module (for TX fan-out)
    std::map<MacNodeId, omnetpp::cModule *> slPhys_;

    // node id -> its SlRrc (for the genie connection-establishment fan-out)
    std::map<MacNodeId, SlRrc *> slRrcs_;

    // cell id -> its SlGnbRrc (D25: serving-cell pool provisioning, SL-3)
    std::map<MacNodeId, SlGnbRrc *> slGnbRrcs_;

    // node id -> shared Uu/SL radio state (D32 half-duplex arbiter, SL-3);
    // owned by the registry (created by SlRrc, deleted with the binder)
    std::map<MacNodeId, SlUeRadioState *> ueRadioStates_;

    // SL carrier registry (G8: not in Binder)
    std::map<GHz, SlCarrierInfo> slCarriers_;

    // SL transmission map (D9), filled from WP-D on
    std::map<GHz, std::map<SlotIndex, std::vector<SlTxRecord>>> slTransmissionMap_;

    void initialize() override {}
    void handleMessage(omnetpp::cMessage *msg) override;

  public:
    ~SlBinder() override
    {
        for (auto& [nodeId, state] : ueRadioStates_)
            delete state;
    }

    /// Find or dynamically create the singleton instance under the network module
    static SlBinder *getInstance();

    // --- L2-ID registry ---
    void registerUeL2Id(SlL2Id srcL2Id, MacNodeId nodeId);
    MacNodeId getNodeIdForL2Id(SlL2Id l2Id) const;     // NODEID_NONE if unknown
    SlL2Id getL2IdForNodeId(MacNodeId nodeId) const;   // SL_L2ID_NONE if unknown

    /// Resolve a destination L2 ID to its L2Pid: an allocated group pseudo id
    /// for broadcast/groupcast, the peer's node id for unicast
    MacNodeId getOrAssignGroupL2Pid(SlL2Id dstL2Id);
    SlL2Id getL2IdForGroupPid(MacNodeId groupPid) const;

    // --- group membership ---
    void joinGroup(SlL2Id dstL2Id, MacNodeId member);
    const std::set<MacNodeId>& getGroupMembers(SlL2Id dstL2Id);
    bool isInGroup(SlL2Id dstL2Id, MacNodeId nodeId);

    // --- IP multicast address mapping ---
    void registerMulticastAddress(inet::Ipv4Address addr, SlL2Id dstL2Id);
    SlL2Id getDstL2IdForMulticastAddress(inet::Ipv4Address addr) const; // SL_L2ID_NONE if unknown

    /// The D16 static PC5-unicast classification rule, shared by the
    /// ip2nic-side modules (SlIp2Nic, SlTechnologyDecision,
    /// SlHandoverPacketHolderUe): true iff destAddr resolves via Binder to a
    /// registered SL-capable UE other than this node. Returns the peer's
    /// node id, or NODEID_NONE.
    MacNodeId getPc5UnicastPeer(Binder *binder, inet::Ipv4Address destAddr, MacNodeId selfNrNodeId) const;

    // --- SL node registry ---
    void registerSlPhy(MacNodeId nodeId, omnetpp::cModule *phyModule);
    const std::map<MacNodeId, omnetpp::cModule *>& getSlPhys() const { return slPhys_; }
    void registerSlRrc(MacNodeId nodeId, SlRrc *slRrc);
    SlRrc *getSlRrc(MacNodeId nodeId) const;  // nullptr if unknown
    void registerSlGnbRrc(MacNodeId cellId, SlGnbRrc *slGnbRrc);
    SlGnbRrc *getSlGnbRrc(MacNodeId cellId) const;  // nullptr if unknown
    void registerUeRadioState(MacNodeId nodeId, SlUeRadioState *state);  // takes ownership
    SlUeRadioState *getUeRadioState(MacNodeId nodeId) const;  // nullptr if unknown

    /// Genie connection establishment for broadcast/groupcast: creates the
    /// outgoing SLRB chain at the sender and the incoming chain at every
    /// group member, via each node's SlRrc. Unicast links are established
    /// symmetrically through SlRrc::establishLink (D17/D18) instead.
    void establishSlConnection(FlowControlInfo *lteInfo);

    // --- SL carrier registry (G8) ---
    void registerSlCarrier(GHz carrierFrequency, unsigned int numerologyIndex, int subchannelSize, int numSubchannels);
    const SlCarrierInfo *getSlCarrier(GHz carrierFrequency) const;  // nullptr if unknown

    // --- SL transmission map (D9) ---
    void recordSlTransmission(GHz carrierFrequency, SlotIndex slot, const SlTxRecord& record, SlotIndex pruneBefore);
    const std::vector<SlTxRecord> *getSlTransmissions(GHz carrierFrequency, SlotIndex slot) const;  // nullptr if none
};

} // namespace simu5g

#endif
