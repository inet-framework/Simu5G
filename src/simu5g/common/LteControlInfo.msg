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

import inet.common.INETDefs;
import inet.common.TagBase;
import simu5g.common.LteCommon;
import simu5g.common.LteCommonEnum;

cplusplus {{
#include "simu5g/common/LteCommon.h"   // for FlowId, BearerRequest
}}

namespace simu5g;

//
// Control info usage in Simu5G simulator
//
// - FlowControlInfo contains information with flow granularity.
//   It is used from Ip2Nic to Mac.
//
// - Between the MAC and the PHY, a packet carries the tags of
//   LteControlInfoTags.msg, one per concern, and what the PHY sends over
//   the air is the TransmissionDescriptor it builds from them.
//
// The Mac layer receives packets (MAC sdu) with FlowControlInfo attached
//
// NOTE: a MAC pdu contains different MAC sdus that may belong to different flows.
//

//
// @class LteControlInfo
// @brief Base class of FlowControlInfo
//
// This class contains the identity of a flow:
// - Source MAC Node ID
// - Destination MAC Node ID
// - D2D MAC Node Ids
// - Traffic Direction
//
class LteControlInfo extends inet::TagBase {
    MacNodeId sourceId = NODEID_NONE;                    // Source MacNodeId
    MacNodeId destId = NODEID_NONE;                      // Destination MacNodeId
    Direction direction = DL;                            // Traffic Direction (UL, DL, D2D)
    MacNodeId d2dTxPeerId = NODEID_NONE;
    MacNodeId d2dRxPeerId = NODEID_NONE;
}

//
// @class FlowControlInfo
// @brief Contains flow information in layers above the MAC
//
class FlowControlInfo extends LteControlInfo {
    DrbId drbId;                                         // DRB (Data Radio Bearer) identifier
    MacNodeId d2dGroupId = NODEID_NONE;                  // D2D groupcast destination: the Binder's id for the multicast IP address; NODEID_NONE = not a groupcast flow
}

cplusplus (FlowControlInfo::setDrbId) {{
    ASSERT(num(drbId) <= MAX_DRB_ID);
}}

cplusplus(FlowControlInfo) {{
    // Build a tag from a FlowId. The tag carries identity only -- bearer configuration
    // (BearerRequest) is a separate, explicit argument at establishment time, never tag state.
    static FlowControlInfo fromFlowId(const FlowId& flow) {
        FlowControlInfo info;
        info.setSourceId(flow.sourceId);
        info.setDestId(flow.destId);
        info.setDirection(flow.direction);
        info.setD2dTxPeerId(flow.d2dTxPeerId);
        info.setD2dRxPeerId(flow.d2dRxPeerId);
        info.setDrbId(flow.drbId);
        info.setD2dGroupId(flow.d2dGroupId);
        return info;
    }
    // Extract the identity half of this tag as a FlowId.
    FlowId toFlowId() const {
        FlowId flow;
        flow.sourceId = getSourceId();
        flow.destId = getDestId();
        flow.direction = (Direction)getDirection();
        flow.d2dTxPeerId = getD2dTxPeerId();
        flow.d2dRxPeerId = getD2dRxPeerId();
        flow.drbId = getDrbId();
        flow.d2dGroupId = getD2dGroupId();
        return flow;
    }
}}

// WIP: Describes logical channels (identified by remote node + LCID) in MAC
class FlowDescriptor extends FlowControlInfo {
}

cplusplus(FlowDescriptor) {{
    static FlowDescriptor fromFlowControlInfo(const FlowControlInfo& ci) {
       return FlowDescriptor(*(FlowDescriptor*)&ci);
    }
    FlowControlInfo toFlowControlInfo() const {
       return FlowControlInfo(*this);
    }
}}

struct FeedbackRequest
{
    bool request;
    FeedbackType type;
    TxMode txMode;
    bool dasAware;
    RbAllocationType rbAllocationType;
};

