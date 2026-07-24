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

#ifndef _LTE_PACKETFLOWSIGNALS_H_
#define _LTE_PACKETFLOWSIGNALS_H_

#include <omnetpp.h>
#include "simu5g/common/LteCommon.h"
#include "simu5g/mec/utils/MecCommon.h"

namespace simu5g {

class LteRlcUmDataPdu;
class LteMacPdu;

/**
 * Signal value struct for RLC PDU creation events (rlcPduCreated signal).
 * Carries the bearer key (peer node + DRB ID -- DRB IDs are only unique per
 * peer, not per node), a pointer to the RLC PDU, and the burst status.
 */
struct RlcPduSignalInfo : public omnetpp::cObject
{
    DrbKey drbKey;
    const LteRlcUmDataPdu *rlcPdu;
    RlcBurstStatus burstStatus;

    RlcPduSignalInfo(DrbKey drbKey, const LteRlcUmDataPdu *rlcPdu, RlcBurstStatus burstStatus)
        : drbKey(drbKey), rlcPdu(rlcPdu), burstStatus(burstStatus) {}
};

/**
 * Signal value struct for RLC PDU discard events (rlcPduDiscarded signal).
 * Carries the bearer key (peer node + DRB ID) and the RLC sequence number.
 */
struct RlcDiscardSignalInfo : public omnetpp::cObject
{
    DrbKey drbKey;
    unsigned int rlcSno;

    RlcDiscardSignalInfo(DrbKey drbKey, unsigned int rlcSno)
        : drbKey(drbKey), rlcSno(rlcSno) {}
};

/**
 * Signal value struct for grant and UL MAC PDU arrival events
 * (grantSent and ulMacPduArrived signals).
 * Carries the MAC node ID and the grant ID.
 */
struct GrantSignalInfo : public omnetpp::cObject
{
    MacNodeId nodeId;
    unsigned int grantId;

    GrantSignalInfo(MacNodeId nodeId, unsigned int grantId)
        : nodeId(nodeId), grantId(grantId) {}
};

/**
 * Signal value struct for MAC PDU events (macPduAcked, macPduDiscarded signals).
 * Carries a plain C pointer to the LteMacPdu header to avoid inet::Ptr overhead.
 */
struct MacPduSignalInfo : public omnetpp::cObject
{
    MacNodeId peerId;   // this direction's destination node; MAC-PDU SDUs carry no
                        // tags (LteMacPdu::pushSdu clears them), so the peer that
                        // scopes the LCID->DRB mapping must ride with the signal
    const LteMacPdu *macPdu;

    MacPduSignalInfo(MacNodeId peerId, const LteMacPdu *macPdu) : peerId(peerId), macPdu(macPdu) {}
};

} //namespace

#endif
