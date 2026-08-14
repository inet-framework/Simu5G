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

#ifndef _SIDELINK_SLRRC_H_
#define _SIDELINK_SLRRC_H_

#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/common/SlPreconfig.h"

namespace simu5g {

class BearerManagement;
class FlowControlInfo;

/**
 * State of one PC5 unicast link (design decision D17). The registry entry is
 * symmetric: both endpoints hold the same DRB ids for the link's SLRBs; each
 * side's entries carry the *other* side's L2 ID as dstL2Id. Under the genie
 * handshake a link is born ESTABLISHED; the over-the-air mode (D23) parks it
 * in ESTABLISHING until the PC5-RRC response arrives (no RLF/keepalive;
 * release only by explicit call).
 */
struct SlUnicastLink
{
    enum State { ESTABLISHING, ESTABLISHED };

    MacNodeId peerId = NODEID_NONE;
    State state = ESTABLISHED;
    std::vector<SlrbConfigEntry> slrbs;   // per-link SLRBs with allocated DRB ids (castType UNICAST)

    /// the SLRB serving a given PFI, or the default entry (WP-J resolution)
    const SlrbConfigEntry& findSlrbForPfi(int pfi) const;
};

/**
 * Per-UE sidelink control plane ("genie" PC5-RRC): owns the sidelink
 * preconfiguration (pool + static SLRB config), registers the UE's L2 IDs /
 * group memberships / multicast address mappings with SlBinder at init, and
 * creates SLRB entity chains via BearerManagement when SlBinder fans out a
 * connection establishment. From SL-2 on it also keeps the PC5 unicast link
 * registry (D17): per-peer links with dynamically allocated per-link SLRBs,
 * established symmetrically (D18) -- the genie handshake runs via SlBinder as
 * direct C++ calls; the over-the-air handshake (D23, WP-L) will drive the
 * same registry with real packets.
 */
class SlRrc : public omnetpp::cSimpleModule
{
  protected:
    SlPreconfig preconfig_;
    SlBinder *slBinder_ = nullptr;
    BearerManagement *bearerManagement_ = nullptr;

    MacNodeId nodeId_ = NODEID_NONE;   // NR node id of the owning UE
    SlL2Id srcL2Id_ = SL_L2ID_NONE;    // this UE's source Layer-2 ID

    // D25 (SL-3): pool provisioning from the serving cell ("SIB12-equivalent")
    bool poolFromServingCell_ = false;
    inet::ModuleRefByPar<Binder> binder_;  // for getServingNode() at pool resolution

    // PC5 unicast link registry (D17), keyed by the peer's node id
    std::map<MacNodeId, SlUnicastLink> links_;

    // over-the-air PC5-RRC (D23)
    bool overTheAir_ = false;
    std::map<MacNodeId, int> srbGates_;       // peer -> srbOut[]/srbIn[] gate index
    std::map<MacNodeId, std::vector<inet::Packet *>> heldPackets_;  // per ESTABLISHING link
    unsigned int pdcpSn_ = 0;                 // PDCP SN counter of the control messages

    void initialize(int stage) override;
    int numInitStages() const override;
    void handleMessage(omnetpp::cMessage *msg) override;

    /// D25/G18: resolve the resource pool (serving cell or local preconfig)
    /// and register the SL carrier; runs at the pool-resolution init stage
    void resolvePool();

    /// create this endpoint's TX and RX chains for every SLRB of the link (D18)
    void createLinkBearers(const SlUnicastLink& link);

    /// allocate the per-link SLRBs from the unicastSlrbDefaults templates
    SlUnicastLink allocateLink(MacNodeId peerId, SlL2Id peerL2Id);

    /// the reserved TM SL-SRB toward a peer (created on first use, D23)
    int ensureSrb(MacNodeId peerId);

    /// wrap and send a PC5-RRC message over the peer's SL-SRB
    void sendPc5RrcMessage(MacNodeId peerId, const inet::Ptr<inet::Chunk>& msg, const char *name);

    void handlePc5RrcMessage(inet::Packet *pkt);
    void flushHeldPackets(MacNodeId peerId);

  public:
    const SlPreconfig& getPreconfig() const { return preconfig_; }
    MacNodeId getNodeId() const { return nodeId_; }
    SlL2Id getSrcL2Id() const { return srcL2Id_; }

    // SLRB entity chain creation (invoked via SlBinder's genie fan-out)
    /// TR 38.885 5.4.2: AM (and the TM SL-SRB) are unicast-only; UM serves every cast type.
    void checkSlrbRlcMode(const FlowId& flow, const BearerRequest& req);
    void createSlOutgoingConnection(const FlowId& flow, const BearerRequest& req);
    void createSlIncomingConnection(const FlowId& flow, const BearerRequest& req);

    // --- PC5 unicast link registry (D17/D18) ---

    /// Establish (or return the existing) unicast link to a peer: allocates
    /// the per-link SLRBs from unicastSlrbDefaults, creates the full TX+RX
    /// chains at BOTH endpoints (symmetric establishment, D18 -- required for
    /// AM: the data-receiver's STATUS PDUs need a co-located reverse TX chain
    /// with its own MAC connection), and registers the link at both ends.
    /// Genie mode: synchronous, triggered by the first classified packet.
    const SlUnicastLink& establishLink(MacNodeId peerId);

    /// Peer-side link setup (called by the initiator's SlRrc under genie):
    /// adopt the initiator's SLRB list and create this side's chains.
    void onLinkRequest(MacNodeId initiatorId, const std::vector<SlrbConfigEntry>& slrbs);

    /// the link to a peer, or nullptr
    const SlUnicastLink *findLink(MacNodeId peerId) const;

    /// hold a data packet while its link is ESTABLISHING (D23); ownership
    /// transfers here, the packet resumes through SlIp2Nic once the link is up
    void holdPacket(MacNodeId peerId, inet::Packet *pkt);
};

} // namespace simu5g

#endif
