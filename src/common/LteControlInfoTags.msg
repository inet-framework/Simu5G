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
import common.LteCommon;

namespace simu5g;

enum LtePhyFrameType;
enum LteRlcType;
enum TxMode;
enum Direction;
enum ApplicationType;
enum LteTrafficClass;

//
// New tag-based control info system for Simu5G
//
// This file contains smaller, focused tag classes that replace the monolithic
// LteControlInfo hierarchy, following INET conventions of using composition
// over inheritance.
//

//
// @class LteNodeIdentityTag
// @brief Contains node identification and direction information
//
// This tag contains the basic routing information needed for packet forwarding
// between nodes in the LTE network.
//
class LteNodeIdentityTag extends inet::TagBase {
    MacNodeId sourceId = NODEID_NONE;                    // Source MacNodeId
    MacNodeId destId = NODEID_NONE;                      // Destination MacNodeId
    unsigned short direction enum(Direction);            // Traffic Direction (UL, DL, D2D)
    MacNodeId d2dTxPeerId = NODEID_NONE;                // D2D transmitter peer ID
    MacNodeId d2dRxPeerId = NODEID_NONE;                // D2D receiver peer ID
}

//
// @class LteConnectionTag
// @brief Contains connection-specific information
//
// This tag contains information about the logical connection and multicast groups.
//
class LteConnectionTag extends inet::TagBase {
    uint16 lcid = 0;                                    // Logical Connection Identifier
    int32 multicastGroupId = -1;                        // Identifier for a multicast group (it is in range [0:[2^28)-1] )
                                                        // For unicast communications, this field is -1
}

//
// @class LteIpFlowTag
// @brief Contains IP-level flow information
//
// This tag contains the IP four-tuple and related information used for
// flow identification and PDCP ROHC header compression.
//
class LteIpFlowTag extends inet::TagBase {
    uint32 srcAddr = 0;                                 // source IP address
    uint32 dstAddr = 0;                                 // destination IP address
    uint16 typeOfService = 0;                           // IP Type Of Service
    unsigned int sequenceNumber = 0;                    // packet sequence number --TODO this is some other seqNum, not IP!
    int headerSize = 0;                                 // IP + Transport header size (used by pdcp ROHC)
}

//
// @class LteTrafficClassificationTag
// @brief Contains traffic classification information
//
// This tag contains information about the application type, traffic class,
// and RLC mode for QoS and scheduling decisions.
//
class LteTrafficClassificationTag extends inet::TagBase {
    unsigned short application enum(ApplicationType) = 0;    // Application Type
    unsigned short traffic enum(LteTrafficClass) = 0;        // Lte Traffic Type
    unsigned short rlcType enum(LteRlcType) = 0;            // RLC Traffic Type
}

//
// @class LteCarrierTag
// @brief Contains carrier and technology information
//
// This tag contains information about which radio technology and carrier
// frequency to use for transmission.
//
class LteCarrierTag extends inet::TagBase {
    bool isNr = false;                                  // indicates if this is an NR transmission
    bool useNR = false;                                 // flag for sending this packet using NR
    double carrierFrequency = NaN;                      // carrier frequency
}

//
// @class LteHarqTag
// @brief Contains HARQ control information
//
// This tag contains all HARQ-related parameters for reliable transmission
// and retransmission handling.
//
class LteHarqTag extends inet::TagBase {
    unsigned char acid = 0;                             // H-ARQ process identifier
    unsigned char cw = 0;                               // H-ARQ codeword identifier
    unsigned char txNumber = 0;                         // number of (re)transmissions for the same pdu (1, 2, 3, 4)
    bool ndi = true;                                    // new data indicator (new data overwrites a process content if present)
}

//
// @class LtePhyTransmissionTag
// @brief Contains PHY transmission parameters
//
// This tag contains physical layer transmission parameters including
// transmission mode, power, and resource allocation information.
//
class LtePhyTransmissionTag extends inet::TagBase {
    unsigned short txMode = 0 enum(TxMode);             // Transmission Mode
    unsigned int frameType = 0 enum(LtePhyFrameType);   // Frame Type
    double txPower = NaN;                               // Transmission Power
    double d2dTxPower = NaN;                            // D2D Transmission Power (used for feedback reporting of D2D links)
    unsigned int totalGrantedBlocks = 0;                // blocks granted on all Remotes, all Bands
    unsigned int grantId = 0;                           // grantId related to the grant that allowed the sending of a MEC PDU (used only for MAC PDU sent by UEs)
}

//
// @class LteQualityTag
// @brief Contains quality and error control information
//
// This tag contains information about packet corruption, broadcast nature,
// and BLER calculations for quality assessment.
//
class LteQualityTag extends inet::TagBase {
    bool isCorruptible = false;                         // Frame is corruptible
    bool isBroadcast = false;                           // Broadcast packet
    bool deciderResult = false;                         // Decider result
    double blerTh = NaN;                                // Calculated BLER threshold for this transmission
    double blerValue = NaN;                             // Extracted BLER for this transmission (blerValue > blerTh ==> corrupted)
}
