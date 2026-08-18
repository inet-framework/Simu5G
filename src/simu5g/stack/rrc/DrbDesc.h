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
// One RLC bearer of a data radio bearer: the leg that carries it in a given cell group
// (TS 38.331 RLC-BearerConfig). A bearer with one leg is an MCG or an SCG bearer; a
// bearer with both is a split bearer.
//
// A leg carries what the configuration stated about it and nothing more: it inherits its
// bearer's RLC mode and overrides it only if its own entry says so, and the fields RRC
// decides for itself at establishment (the wire format and the SN space, see
// BearerManagement::materializeDrb()) stay unset unless a leg overrides those too. An
// unset field is not the same as a defaulted one -- keeping them apart is what lets
// establishment go on deciding whatever the configuration did not.
//
struct RlcBearerDesc {
    CellGroup cellGroup = MCG;              // which cell group serves this leg
    LteRlcType rlcType = UNKNOWN_RLC_TYPE;  // rlc-Config: TM, UM or AM; UNKNOWN = not stated, RRC decides from the QoS class
    std::optional<bool> soFraming;          // wire format: false = LTE FI/concatenation (TS 36.322), true = NR SI/SO (TS 38.322)
    std::optional<unsigned int> snFieldLength;   // sn-FieldLength, in bits
};

inline std::ostream& operator<<(std::ostream& os, const RlcBearerDesc& leg) {
    os << cellGroupToA(leg.cellGroup);
    if (leg.rlcType != UNKNOWN_RLC_TYPE) os << " " << rlcTypeToA(leg.rlcType);
    if (leg.soFraming) os << (*leg.soFraming ? " SO" : " FI");
    if (leg.snFieldLength) os << " snBits=" << *leg.snFieldLength;
    return os;
}

//
// Configuration of a data radio bearer, owned by RRC (see ~DrbTable).
//
// Mirrors TS 38.331: the SDAP half of DRB-ToAddMod (cnAssociation -> SDAP-Config), and
// the RLC-BearerConfig of the bearer's logical channel (RLC mode, wire format, SN field
// length, logical channel identity and group).
//
// A split bearer (NR dual connectivity) is served by one RLC bearer per cell group, which
// the standard keeps in a list joined on drb-Identity. The legs field is that list, and
// the configuration can author it. Establishment does not read it yet: it still derives
// the legs a bearer gets, and describes a split bearer as one descriptor per leg, each
// keyed by the peer of that leg. Moving establishment onto the authored legs -- and with
// it the RLC-BearerConfig fields below, which the standard keeps per RLC bearer rather
// than per DRB -- is the next step.
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

    // The bearer's RLC bearers, one per cell group it is served in (TS 38.331 keeps them
    // in a list joined on drb-Identity). Empty = the configuration did not state them and
    // RRC derives the bearer's legs, as it always has. Authored, not yet consumed: the
    // legs a bearer is established with still come from BearerManagement::getNumLegs().
    std::vector<RlcBearerDesc> legs;

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
    if (!drb.legs.empty()) {
        os << " legs=[";
        for (size_t i = 0; i < drb.legs.size(); i++) {
            if (i) os << ",";
            os << drb.legs[i];
        }
        os << "]";
    }
    os << " rlc=" << rlcTypeToA(drb.rlcType) << (drb.soFraming ? " SO" : " FI")
       << " snBits=" << drb.snFieldLength
       << " lcid=" << num(drb.lcid) << " lcg=" << lteTrafficClassToA(drb.lcg);
    if (drb.hasQosProfile)
        os << " qos: " << drb.qos;
    return os;
}

} // namespace simu5g

#endif
