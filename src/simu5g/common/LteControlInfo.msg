//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

import inet.common.INETDefs;
import inet.common.TagBase;
import simu5g.common.LteCommon;
import simu5g.common.LteCommonImpl;
import inet.common.geometry.Geometry; // for Coord

cplusplus {{
#include "inet/common/ModuleAccess.h"  // for findContainingNode()
#include "simu5g/stack/mac/amc/UserTxParams.h"
}}

cplusplus {{
inline const char *dirOnStack(bool down, bool up) {
    if (down && up)
        throw cRuntimeError("BOTH DIRS!!!");
    if (down)
        return "DOWN";
    else if (up)
        return "UP";
    else
        throw cRuntimeError("DIR NOT FILLED IN");
}

#define LOG_FIELD_ACCESS(field, access)  \
  std::cout << "@ " << field << " " << getClassName() << " " << dirOnStack(isDownStack(), isUpStack()) << " " << access \
            << " by " << getSimulation()->getContextModule()->getClassName() \
            << " in " << inet::findContainingNode(getSimulation()->getContextModule())->getModuleType()->getName() << std::endl;
#define LOG_FIELD_READ(field)   LOG_FIELD_ACCESS(field, "READ")
#define LOG_FIELD_WRITE(field)  LOG_FIELD_ACCESS(field, "WRITE")
}}

namespace simu5g;

enum LtePhyFrameType;
enum LteRlcType;
enum TxMode;
enum Direction;
enum LteTrafficClass;

//
// Control info usage in Simu5G simulator
//
// - FlowControlInfo contains information with flow granularity.
//   It is used from Ip2Nic to Mac.
//
// - UserControlInfo contains information with user granularity.
//   It is used from Mac to Phy.
//
// Both FlowControlInfo and UserControlInfo inherit from LteControlInfo
// to avoid replication of common parts.
//
// The Mac layer receives packets (MAC sdu) with FlowControlInfo attached
// and creates a MAC pdu with UserControlInfo attached.
// The MacPduMaker does the job.
//
// NOTE: a MAC pdu contains different MAC sdus that may belong to different flows.
//

//
// @class LteControlInfo
// @brief Base class for FlowControlInfo and UserControlInfo
//
// This class contains information present in both Flow and User ControlInfo:
// - Source MAC Node ID
// - Destination MAC Node ID
//
class LteControlInfo extends inet::TagBase {
    // TMP fields to allow tracking of control info usage
    bool isDownStack = false;
    bool isUpStack = false;

    //# Information shared between FlowControlInfo and UserControlInfo

    MacNodeId sourceId = NODEID_NONE;                    // Source MacNodeId
    MacNodeId destId = NODEID_NONE;                      // Destination MacNodeId
    unsigned short direction enum(Direction);            // Traffic Direction (UL, DL, D2D)
    MacNodeId d2dTxPeerId = NODEID_NONE;
    MacNodeId d2dRxPeerId = NODEID_NONE;

    //# Traffic Control Information

    unsigned short traffic enum(LteTrafficClass) = 0;        // Lte Traffic Type

    //# RLC Control Information

    unsigned short rlcType enum(LteRlcType) = 0;            // RLC Traffic Type

    //# Connection Information

    uint16 lcid = 0;                                        // Logical Connection Identifier
    int32 multicastGroupId = -1;                        // Identifier for a multicast group (it is in range [0:[2^28)-1] )
                                                        // For unicast communications, this field is -1
}

//
// @class FlowControlInfo
// @brief contains information flowing from LteIp to Mac
//
// It contains the following information:
// - Traffic Control Informations: Application, Direction, Traffic
// - RLC Control Informations: the traffic type (TM, UM, AM)
// - Connection information: Logical CID
//
// Note: IP Control Information (srcAddr, dstAddr, typeOfService) is now handled by IpFlowInd tag
// Note: Technology selection (useNR) is now handled by TechnologyReq tag
//
class FlowControlInfo extends LteControlInfo {
    //# IP Control Information

    unsigned int sequenceNumber = 0;        // packet sequence number
}

// Describes logical channels (identified by remote node + LCID) in MAC
class FlowDescriptor extends LteControlInfo {
}

cplusplus(FlowDescriptor) {{
    static FlowDescriptor fromFlowControlInfo(const FlowControlInfo& ci) {
       FlowDescriptor result;
       static_cast<LteControlInfo&>(result) = static_cast<const LteControlInfo&>(ci);
       return result;
    }
    FlowControlInfo toFlowControlInfo() const {
       FlowControlInfo result;
       static_cast<LteControlInfo&>(result) = static_cast<const LteControlInfo&>(*this);
       return result;
    }

}}

//
// @class UserControlInfo
// @brief contains information flowing from Mac to Phy
//
// It contains the following information:
// - H-ARQ Control Information
// - PHY Control Informations: Broadcast, Corruption, Type,
//   txMode, Resource blocks used, RemoteSet
//
class UserControlInfo extends LteControlInfo {
    bool isNr = false;
    double carrierFrequency = NaN;       // carrier frequency

    //# H-ARQ Control Information

    unsigned char acid = 0;              // H-ARQ process identifier
    unsigned char cw = 0;                // H-ARQ codeword identifier
    unsigned char txNumber = 0;          // number of (re)transmissions for the same pdu (1, 2, 3, 4)
    bool ndi = true;                     // new data indicator (new data overwrites a process content if present)

    //# PHY Control Informations

    bool deciderResult = false;                         // Decider result
    unsigned short txMode = 0 enum(TxMode);             // Traffic Type
    unsigned int frameType = 0 enum(LtePhyFrameType);   // Frame Type
    double txPower = NaN;                               //Transmission Power
    double d2dTxPower = NaN;                            // D2D Transmission Power (used for feedback reporting of D2D links
    // blocks granted on all Remotes, all Bands
    unsigned int totalGrantedBlocks = 0;                // note: currently unread
    unsigned int grantId = 0;                           // grantId related to the grand that allowed the sending of a MEC PDU (used only for MAC PDU sent by UEs)

    UserTxParams *userTxParams @owned;
    RbMap grantedBlocks;
    inet::Coord coord;                                  // The playground position of the sending host
    FeedbackRequest feedbackReq;
}

cplusplus (UserControlInfo) {{
    virtual const unsigned int getBlocks(Remote antenna, Band b) const { return grantedBlocks.at(antenna).at(b); }
    virtual void setBlocks(Remote antenna, Band b, const unsigned int blocks) { grantedBlocks[antenna][b] = blocks; }
}}

cplusplus (UserControlInfo::setUserTxParams) {{
    // Code inserted at top of setUserTxParams(); allows silent overwrite of earlier object.
    delete this->userTxParams;
    this->userTxParams = nullptr;
}}

// LteControlInfo field logging

cplusplus (LteControlInfo::setSourceId) {{
    LOG_FIELD_WRITE("sourceId")
}}
cplusplus (LteControlInfo::getSourceId) {{
    LOG_FIELD_READ("sourceId")
}}

cplusplus (LteControlInfo::setDestId) {{
    LOG_FIELD_WRITE("destId")
}}
cplusplus (LteControlInfo::getDestId) {{
    LOG_FIELD_READ("destId")
}}

cplusplus (LteControlInfo::setDirection) {{
    LOG_FIELD_WRITE("direction")
}}
cplusplus (LteControlInfo::getDirection) {{
    LOG_FIELD_READ("direction")
}}

cplusplus (LteControlInfo::setD2dTxPeerId) {{
    LOG_FIELD_WRITE("d2dTxPeerId")
}}
cplusplus (LteControlInfo::getD2dTxPeerId) {{
    LOG_FIELD_READ("d2dTxPeerId")
}}

cplusplus (LteControlInfo::setD2dRxPeerId) {{
    LOG_FIELD_WRITE("d2dRxPeerId")
}}
cplusplus (LteControlInfo::getD2dRxPeerId) {{
    LOG_FIELD_READ("d2dRxPeerId")
}}

cplusplus (LteControlInfo::setTraffic) {{
    LOG_FIELD_WRITE("traffic")
}}
cplusplus (LteControlInfo::getTraffic) {{
    LOG_FIELD_READ("traffic")
}}

cplusplus (LteControlInfo::setRlcType) {{
    LOG_FIELD_WRITE("rlcType")
}}
cplusplus (LteControlInfo::getRlcType) {{
    LOG_FIELD_READ("rlcType")
}}

cplusplus (LteControlInfo::setLcid) {{
    LOG_FIELD_WRITE("lcid")
}}
cplusplus (LteControlInfo::getLcid) {{
    LOG_FIELD_READ("lcid")
}}

cplusplus (LteControlInfo::setMulticastGroupId) {{
    LOG_FIELD_WRITE("multicastGroupId")
}}
cplusplus (LteControlInfo::getMulticastGroupId) {{
    LOG_FIELD_READ("multicastGroupId")
}}

// FlowControlInfo field logging
cplusplus (FlowControlInfo::setSequenceNumber) {{
    LOG_FIELD_WRITE("sequenceNumber")
}}
cplusplus (FlowControlInfo::getSequenceNumber) {{
    LOG_FIELD_READ("sequenceNumber")
}}

// UserControlInfo field logging
cplusplus (UserControlInfo::setIsNr) {{
    LOG_FIELD_WRITE("isNr")
}}
cplusplus (UserControlInfo::isNr) {{
    LOG_FIELD_READ("isNr")
}}

cplusplus (UserControlInfo::setCarrierFrequency) {{
    LOG_FIELD_WRITE("carrierFrequency")
}}
cplusplus (UserControlInfo::getCarrierFrequency) {{
    LOG_FIELD_READ("carrierFrequency")
}}

cplusplus (UserControlInfo::setAcid) {{
    LOG_FIELD_WRITE("acid")
}}
cplusplus (UserControlInfo::getAcid) {{
    LOG_FIELD_READ("acid")
}}

cplusplus (UserControlInfo::setCw) {{
    LOG_FIELD_WRITE("cw")
}}
cplusplus (UserControlInfo::getCw) {{
    LOG_FIELD_READ("cw")
}}

cplusplus (UserControlInfo::setTxNumber) {{
    LOG_FIELD_WRITE("txNumber")
}}
cplusplus (UserControlInfo::getTxNumber) {{
    LOG_FIELD_READ("txNumber")
}}

cplusplus (UserControlInfo::setNdi) {{
    LOG_FIELD_WRITE("ndi")
}}
cplusplus (UserControlInfo::getNdi) {{
    LOG_FIELD_READ("ndi")
}}

cplusplus (UserControlInfo::setDeciderResult) {{
    LOG_FIELD_WRITE("deciderResult")
}}
cplusplus (UserControlInfo::getDeciderResult) {{
    LOG_FIELD_READ("deciderResult")
}}

cplusplus (UserControlInfo::setTxMode) {{
    LOG_FIELD_WRITE("txMode")
}}
cplusplus (UserControlInfo::getTxMode) {{
    LOG_FIELD_READ("txMode")
}}

cplusplus (UserControlInfo::setFrameType) {{
    LOG_FIELD_WRITE("frameType")
}}
cplusplus (UserControlInfo::getFrameType) {{
    LOG_FIELD_READ("frameType")
}}

cplusplus (UserControlInfo::setTxPower) {{
    LOG_FIELD_WRITE("txPower")
}}
cplusplus (UserControlInfo::getTxPower) {{
    LOG_FIELD_READ("txPower")
}}

cplusplus (UserControlInfo::setD2dTxPower) {{
    LOG_FIELD_WRITE("d2dTxPower")
}}
cplusplus (UserControlInfo::getD2dTxPower) {{
    LOG_FIELD_READ("d2dTxPower")
}}

cplusplus (UserControlInfo::setTotalGrantedBlocks) {{
    LOG_FIELD_WRITE("totalGrantedBlocks")
}}
cplusplus (UserControlInfo::getTotalGrantedBlocks) {{
    LOG_FIELD_READ("totalGrantedBlocks")
}}

cplusplus (UserControlInfo::setGrantId) {{
    LOG_FIELD_WRITE("grantId")
}}
cplusplus (UserControlInfo::getGrantId) {{
    LOG_FIELD_READ("grantId")
}}

cplusplus (UserControlInfo::getUserTxParams) {{
    LOG_FIELD_READ("userTxParams")
}}

cplusplus (UserControlInfo::setGrantedBlocks) {{
    LOG_FIELD_WRITE("grantedBlocks")
}}
cplusplus (UserControlInfo::getGrantedBlocks) {{
    LOG_FIELD_READ("grantedBlocks")
}}

cplusplus (UserControlInfo::setCoord) {{
    LOG_FIELD_WRITE("coord")
}}
cplusplus (UserControlInfo::getCoord) {{
    LOG_FIELD_READ("coord")
}}

cplusplus (UserControlInfo::setFeedbackReq) {{
    LOG_FIELD_WRITE("feedbackReq")
}}
cplusplus (UserControlInfo::getFeedbackReq) {{
    LOG_FIELD_READ("feedbackReq")
}}

