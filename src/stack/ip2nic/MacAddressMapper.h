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

#ifndef __MACADDRESSMAPPER_H_
#define __MACADDRESSMAPPER_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>
#include <inet/linklayer/common/MacAddressTag_m.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include <inet/networklayer/arp/ipv4/ArpPacket_m.h>
#include <inet/networklayer/contract/IArp.h>
#include <inet/networklayer/contract/IInterfaceTable.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/ModuleRefByPar.h>
#include "common/LteCommon.h"
#include "common/LteControlInfo.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @class MacAddressMapper
 * @brief Protocol translator for integrating LTE/5G MAC layers as Ethernet switch ports
 *
 * This module enables converged networking by allowing cellular base stations to appear
 * as Ethernet switch ports. It provides bidirectional protocol translation between
 * IEEE 802 Ethernet and LTE/5G cellular domains.
 *
 * Ethernet → Cellular (Downstream):
 * - Receives Ethernet frames from switch infrastructure
 * - Extracts IEEE 802 MAC addresses and maps to LTE destId for cellular routing
 * - Enables Ethernet switches to route traffic to cellular endpoints
 *
 * Cellular → Ethernet (Upstream):
 * - Receives packets from cellular stack with LTE control information
 * - Generates IEEE 802 MAC address indications for Ethernet switching
 * - Resolves destinations via ARP across all broadcast interfaces
 * - Enables cellular traffic to be routed through standard Ethernet switches
 *
 * This enables revolutionary network convergence where LTE/5G base stations
 * integrate seamlessly into existing Ethernet switching infrastructure.
 */
class MacAddressMapper : public cSimpleModule, public cListener
{
  protected:
    // Module references
    inet::ModuleRefByPar<inet::IArp> arp;
    inet::ModuleRefByPar<inet::IInterfaceTable> ift;

    // Pending packets waiting for ARP resolution
    std::map<inet::Ipv4Address, cPacketQueue> pendingPackets;

    /**
     * Initialize the module
     */
    virtual void initialize() override;

    /**
     * Handle incoming messages
     */
    virtual void handleMessage(cMessage *msg) override;

    /**
     * Handle signals from ARP module
     */
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;

    /**
     * Process packets from upper layer (IP2Nic)
     * Extracts MAC address and sets destId in LteControlInfo
     */
    virtual void handleUpperLayerPacket(inet::Packet *pkt);

    /**
     * Process packets from lower layer (PDCP-RRC)
     * Generates IEEE 802 MAC address indications and forwards to upper layer
     */
    virtual void handleLowerLayerPacket(inet::Packet *pkt);

    /**
     * Extract destination MAC address from packet tags and set destId
     */
    virtual void processMacAddressMapping(inet::Packet *pkt);

    /**
     * Extract lower 16 bits from MAC address
     */
    virtual MacNodeId extractDestIdFromMac(const inet::MacAddress& macAddr);

    /**
     * Generate IEEE 802 MAC address indications for upstream packets
     */
    virtual void generateMacAddressIndications(inet::Packet *pkt);

    /**
     * Create source MAC address from srcId (16 bits) with hardcoded prefix
     */
    virtual inet::MacAddress createSrcMacFromNodeId(MacNodeId srcId);

    /**
     * Resolve destination IPv4 address to MAC address via ARP
     */
    virtual inet::MacAddress resolveDestMacFromIpv4(const inet::Ipv4Address& destIp);

  private:
    // Hardcoded constant for upper 32 bits of MAC address
    static const uint64_t MAC_PREFIX = 0x020000000000ULL; // Local administered unicast

  public:
    MacAddressMapper() {}
    virtual ~MacAddressMapper();
};

} //namespace

#endif
