//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _DRB_DESC_H_
#define _DRB_DESC_H_

#include <string>
#include <vector>

#include "simu5g/common/LteCommon.h"
#include "simu5g/stack/mac/DrbQosProfile.h"

namespace simu5g {

//
// Configuration of a data radio bearer, owned by RRC (see ~DrbTable).
//
// Mirrors TS 38.331: the SDAP half of DRB-ToAddMod (cnAssociation -> SDAP-Config), and
// the RLC-BearerConfig of the bearer's logical channel (RLC mode, wire format, SN field
// length, logical channel identity and group).
//
// A split bearer (NR dual connectivity) is served by one RLC bearer per cell group, which
// the standard keeps in a list joined on drb-Identity. This descriptor has room for one:
// a split bearer is described by one descriptor per leg, each keyed by the peer of that
// leg. Making the legs a table of one bearer is a later step.
//
struct DrbDesc {
    DrbKey key;                         // (peer node, DRB id); the peer is the UE at a gNB, the serving node at a UE

    // Which bearer architecture selects this bearer: BEARER_5GC = QoS flows via
    // qfiList (SDAP), BEARER_EPS = packet filters via filters (no SDAP). Authored
    // entries always state it; descriptors materialized from establishment alone
    // keep UNKNOWN_BEARER_TYPE.
    BearerType bearerType = UNKNOWN_BEARER_TYPE;

    // SDAP-Config
    PduSessionType pduSessionType = IP_V4;
    std::string upperProtocol;          // INET protocol name for upper-layer dispatch (empty = derive from pduSessionType)
    std::vector<Qfi> qfiList;           // mappedQoS-FlowsToAdd
    bool isDefault = false;             // defaultDRB (5gc: fallback for unmapped QFIs; eps: carries traffic matching no filter)

    // Packet filters selecting this bearer (BEARER_EPS only; inet::PacketFilter
    // syntax: a message-name pattern, or an expression written as "expr(...)")
    std::vector<std::string> filters;

    // RLC-BearerConfig
    LteRlcType rlcType = UM;            // rlc-Config: TM, UM or AM
    bool soFraming = false;             // wire format: false = LTE FI/concatenation (TS 36.322), true = NR SI/SO (TS 38.322)
    unsigned int snFieldLength = 12;    // sn-FieldLength, in bits
    LogicalCid lcid = LCID_NONE;        // logicalChannelIdentity
    LteTrafficClass lcg = CONVERSATIONAL;   // logicalChannelGroup, mac-LogicalChannelConfig

    // QoS profile of the bearer's flows (5QI characteristics), for QoS-aware MAC
    // scheduling; only meaningful when hasQosProfile is set (an authored entry
    // carried at least one qos field)
    bool hasQosProfile = false;
    DrbQosProfile qos;

    DrbId getDrbId() const { return key.getDrbId(); }
    MacNodeId getPeerId() const { return key.getNodeId(); }
};

inline std::ostream& operator<<(std::ostream& os, const DrbDesc& drb) {
    os << "drbId=" << drb.getDrbId() << " peer=" << drb.getPeerId();
    if (drb.bearerType != UNKNOWN_BEARER_TYPE) os << " " << bearerTypeToA(drb.bearerType);
    if (drb.isDefault) os << " DEFAULT";
    os << " qfi=[";
    for (size_t i = 0; i < drb.qfiList.size(); i++) {
        if (i) os << ",";
        os << drb.qfiList[i];
    }
    os << "]";
    if (!drb.filters.empty()) {
        os << " filters=[";
        for (size_t i = 0; i < drb.filters.size(); i++) {
            if (i) os << ",";
            os << "\"" << drb.filters[i] << "\"";
        }
        os << "]";
    }
    os << " pduSession=" << pduSessionTypeToA(drb.pduSessionType);
    if (!drb.upperProtocol.empty())
        os << " upperProto=" << drb.upperProtocol;
    os << " rlc=" << rlcTypeToA(drb.rlcType) << (drb.soFraming ? " SO" : " FI")
       << " snBits=" << drb.snFieldLength
       << " lcid=" << num(drb.lcid) << " lcg=" << lteTrafficClassToA(drb.lcg);
    if (drb.hasQosProfile)
        os << " qos: " << drb.qos;
    return os;
}

} // namespace simu5g

#endif
