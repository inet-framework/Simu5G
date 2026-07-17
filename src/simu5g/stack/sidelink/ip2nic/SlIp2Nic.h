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

#ifndef _SIDELINK_SLIP2NIC_H_
#define _SIDELINK_SLIP2NIC_H_

#include "simu5g/stack/ip2nic/Ip2Nic.h"

namespace simu5g {

class SlBinder;
class SlRrc;

/**
 * IP-to-NIC bridge for sidelink-capable UEs (fixes gap G2): packets destined
 * to a registered PC5 destination are classified as SL flows (direction SL,
 * L2 IDs, destination L2Pid, SLRB from the preconfiguration) and their SLRB
 * chain is genie-established on first use; all other traffic gets the base
 * behavior.
 *
 * From SL-2 on (D16), unicast packets whose destination address maps to an
 * SL-capable peer are classified as PC5 unicast when pc5UnicastEnabled: a
 * deliberately static path-selection rule ("PC5 whenever the peer is
 * SL-capable"); the Uu/PC5 policy hook stays in SL-3. The first classified
 * packet triggers the PC5 unicast link establishment (D17).
 */
class BearerManagement;

class SlIp2Nic : public Ip2Nic
{
  protected:
    SlBinder *slBinder_ = nullptr;
    SlRrc *slRrc_ = nullptr;
    BearerManagement *bearerManagement_ = nullptr;
    bool pc5UnicastEnabled_ = false;
    bool hasSlSdap_ = false;
    bool useDscpAsPfiFallback_ = false;

    // peer key -> SlIp2Nic-side gate index of the entity's side chain (D20)
    std::map<uint32_t, int> sdapTxGates_;
    std::map<uint32_t, int> sdapRxGates_;

    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void toStackUe(inet::Packet *pkt) override;
    void analyzePacket(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, uint16_t typeOfService) override;

    /// stamp a packet for the PC5 unicast link to a peer, establishing the
    /// link on first use (D16/D17; synchronous under the genie handshake)
    virtual void analyzeUnicastPc5Packet(inet::Packet *pkt, MacNodeId peerId);

    // --- SL-SDAP side chain (D20) ---

    /// PFI of a TX packet: QfiReq tag, else DSCP fallback, else 0
    virtual int resolvePfi(inet::Packet *pkt);

    /// hand a classified TX packet to the destination's SDAP entity
    /// (created on first use)
    virtual void routeViaSdapTx(inet::Packet *pkt, uint32_t dstL2Id);

    /// hand a received SL packet to the source's SDAP entity
    virtual void routeViaSdapRx(inet::Packet *pkt, uint32_t srcPid);

    /// TX packet returned from its entity: establish the (now final) SLRB
    /// if needed, continue to the stack
    virtual void onSdapTxReturn(inet::Packet *pkt);

    /// RX packet returned from its entity: continue the base stackIn
    /// processing (tag strip, hand to IP)
    virtual void onSdapRxReturn(inet::Packet *pkt);
};

} // namespace simu5g

#endif
