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

#include <inet/networklayer/common/NetworkInterface.h>
#include "stack/ip2nic/MacAddressMapper.h"

namespace simu5g {

Define_Module(MacAddressMapper);

void MacAddressMapper::initialize()
{
    EV << "MacAddressMapper::initialize" << endl;

    // Get references to ARP and interface table modules
    arp.reference(this, "arpModule", true);
    ift.reference(this, "interfaceTableModule", true);

    // Subscribe to ARP resolution signals
    cModule *arpModule = check_and_cast<cModule *>(arp.get());
    arpModule->subscribe(inet::IArp::arpResolutionCompletedSignal, this);
    arpModule->subscribe(inet::IArp::arpResolutionFailedSignal, this);
}

void MacAddressMapper::handleMessage(cMessage *msg)
{
    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);

    cGate *arrivalGate = pkt->getArrivalGate();

    if (strcmp(arrivalGate->getName(), "upperLayerIn") == 0)
    {
        // Packet from IP2Nic - process MAC address mapping
        handleUpperLayerPacket(pkt);
    }
    else if (strcmp(arrivalGate->getName(), "lowerLayerIn") == 0)
    {
        // Packet from PDCP-RRC - forward unchanged
        handleLowerLayerPacket(pkt);
    }
    else
    {
        EV_ERROR << "MacAddressMapper: Unknown arrival gate: " << arrivalGate->getName() << endl;
        delete pkt;
    }
}

void MacAddressMapper::handleUpperLayerPacket(inet::Packet *pkt)
{
    EV << "MacAddressMapper::handleUpperLayerPacket - processing Ethernet frame for cellular routing" << endl;

    // Process Ethernet → Cellular translation: extract IEEE 802 MAC and map to LTE destId
    processMacAddressMapping(pkt);

    // Forward to cellular stack (PDCP-RRC)
    send(pkt, "lowerLayerOut");
}

void MacAddressMapper::handleLowerLayerPacket(inet::Packet *pkt)
{
    EV << "MacAddressMapper::handleLowerLayerPacket - processing cellular packet for Ethernet switching" << endl;

    // Process Cellular → Ethernet translation: generate IEEE 802 MAC address indications
    generateMacAddressIndications(pkt);

    // Forward to Ethernet domain for switch processing
    send(pkt, "upperLayerOut");
}

void MacAddressMapper::processMacAddressMapping(inet::Packet *pkt)
{
    auto macAddressReq = pkt->getTag<inet::MacAddressReq>(); // must exist on packet

    inet::MacAddress destMac = macAddressReq->getDestAddress();
    MacNodeId destId = extractDestIdFromMac(destMac);

    auto lteControlInfo = pkt->addTagIfAbsent<FlowControlInfo>();
    lteControlInfo->setDestId(destId);

    EV << "Mapping destination MAC address " << destMac << " to destId=" << destId << endl;
}

MacNodeId MacAddressMapper::extractDestIdFromMac(const inet::MacAddress& macAddr)
{
    // Get the MAC address as a 64-bit integer
    uint64_t macInt = macAddr.getInt();

    // Extract lower 16 bits
    MacNodeId destId = static_cast<MacNodeId>(macInt & 0xFFFF);

    EV << "MacAddressMapper: MAC " << macAddr << " -> destId " << destId << endl;

    return destId;
}

void MacAddressMapper::generateMacAddressIndications(inet::Packet *pkt)
{
    // Look for LteControlInfo to get source node ID
    auto lteControlInfo = pkt->getTag<FlowControlInfo>();

    MacNodeId srcId = lteControlInfo->getSourceId();
    EV << "MacAddressMapper: Found srcId: " << srcId << " in LteControlInfo" << endl;

    // Create source MAC address from srcId (16 bits) with hardcoded prefix
    inet::MacAddress srcMac = createSrcMacFromNodeId(srcId);
    EV << "MacAddressMapper: Generated source MAC: " << srcMac << endl;

    // Try to extract IPv4 header to get destination IP
    try {
        auto ipHeader = pkt->peekAtFront<inet::Ipv4Header>();
        inet::Ipv4Address destIp = ipHeader->getDestAddress();
        EV << "MacAddressMapper: Found destination IP: " << destIp << endl;

        // Resolve destination IP to MAC address via ARP
        inet::MacAddress destMac = resolveDestMacFromIpv4(destIp);

        if (destMac.isUnspecified()) {
            // ARP resolution is pending, queue the packet
            EV_INFO << "MacAddressMapper: Queuing packet for ARP resolution of " << destIp << endl;
            if (pendingPackets.find(destIp) == pendingPackets.end()) {
                pendingPackets[destIp] = cPacketQueue();
            }
            pendingPackets[destIp].insert(pkt);
            return; // Don't forward the packet yet
        }

        EV << "MacAddressMapper: Resolved destination MAC: " << destMac << endl;

        // Add MAC address indication tag
        auto macAddressInd = pkt->addTag<inet::MacAddressInd>();
        macAddressInd->setSrcAddress(srcMac);
        macAddressInd->setDestAddress(destMac);
        EV << "MacAddressMapper: Added MacAddressInd tag with src=" << srcMac << ", dest=" << destMac << endl;
    }
    catch (const std::exception& e) {
        EV_WARN << "MacAddressMapper: Could not extract IPv4 header: " << e.what() << endl;
    }
}

inet::MacAddress MacAddressMapper::createSrcMacFromNodeId(MacNodeId srcId)
{
    // Create MAC address with hardcoded prefix (upper 32 bits) and srcId (lower 16 bits)
    uint64_t macInt = MAC_PREFIX | static_cast<uint64_t>(srcId);
    inet::MacAddress srcMac(macInt);

    EV << "MacAddressMapper: Created MAC " << srcMac << " from srcId " << srcId << endl;

    return srcMac;
}

inet::MacAddress MacAddressMapper::resolveDestMacFromIpv4(const inet::Ipv4Address& destIp)
{
    // Try ARP resolution on all broadcast interfaces
    inet::MacAddress resolvedMac = inet::MacAddress::UNSPECIFIED_ADDRESS;
    bool arpStarted = false;

    for (int i = 0; i < ift->getNumInterfaces(); i++) {
        const inet::NetworkInterface *ie = ift->getInterface(i);

        // Skip loopback interfaces and non-broadcast interfaces
        if (ie->isLoopback() || !ie->isBroadcast()) {
            continue;
        }

        EV << "MacAddressMapper: Trying ARP resolution on interface " << ie->getInterfaceName() << " for " << destIp << endl;

        // Try to resolve via ARP on this interface
        inet::MacAddress interfaceResolvedMac = arp->resolveL3Address(destIp, ie);

        if (!interfaceResolvedMac.isUnspecified()) {
            // Found a resolved MAC address
            resolvedMac = interfaceResolvedMac;
            EV << "MacAddressMapper: ARP resolved IP " << destIp << " to MAC " << resolvedMac
               << " on interface " << ie->getInterfaceName() << endl;
            return resolvedMac;
        } else {
            // ARP resolution started on this interface
            arpStarted = true;
            EV << "MacAddressMapper: ARP resolution started on interface " << ie->getInterfaceName() << " for " << destIp << endl;
        }
    }

    if (arpStarted) {
        EV_INFO << "MacAddressMapper: ARP resolution pending for " << destIp << " on broadcast interfaces" << endl;
        // ARP resolution is in progress on one or more interfaces
        return inet::MacAddress::UNSPECIFIED_ADDRESS;
    }

    // No broadcast interfaces found, throw error
    throw cRuntimeError("MacAddressMapper: No broadcast interfaces found for ARP resolution");
}

void MacAddressMapper::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == inet::IArp::arpResolutionCompletedSignal) {
        auto *entry = check_and_cast<inet::IArp::Notification *>(obj);
        if (entry->l3Address.getType() == inet::L3Address::IPv4) {
            inet::Ipv4Address resolvedIp = entry->l3Address.toIpv4();
            auto it = pendingPackets.find(resolvedIp);
            if (it != pendingPackets.end()) {
                cPacketQueue& packetQueue = it->second;
                EV << "MacAddressMapper: ARP resolution completed for " << resolvedIp
                   << ". Processing " << packetQueue.getLength() << " waiting packets" << endl;

                while (!packetQueue.isEmpty()) {
                    inet::Packet *packet = check_and_cast<inet::Packet *>(packetQueue.pop());
                    EV << "MacAddressMapper: Processing queued packet " << packet << endl;

                    // Retry generating MAC address indications with resolved MAC
                    generateMacAddressIndications(packet);

                    // Forward to upper layer
                    send(packet, "upperLayerOut");
                }
                pendingPackets.erase(it);
            }
        }
    }
    else if (signalID == inet::IArp::arpResolutionFailedSignal) {
        auto *entry = check_and_cast<inet::IArp::Notification *>(obj);
        if (entry->l3Address.getType() == inet::L3Address::IPv4) {
            inet::Ipv4Address failedIp = entry->l3Address.toIpv4();
            auto it = pendingPackets.find(failedIp);
            if (it != pendingPackets.end()) {
                cPacketQueue& packetQueue = it->second;
                EV_WARN << "MacAddressMapper: ARP resolution failed for " << failedIp
                        << ". Dropping " << packetQueue.getLength() << " packets" << endl;

                // Drop all pending packets for this IP
                while (!packetQueue.isEmpty()) {
                    inet::Packet *packet = check_and_cast<inet::Packet *>(packetQueue.pop());
                    delete packet;
                }
                pendingPackets.erase(it);
            }
        }
    }
}

MacAddressMapper::~MacAddressMapper()
{
    // Clean up pending packets
    for (auto& entry : pendingPackets) {
        cPacketQueue& packetQueue = entry.second;
        while (!packetQueue.isEmpty()) {
            inet::Packet *packet = check_and_cast<inet::Packet *>(packetQueue.pop());
            delete packet;
        }
    }
    pendingPackets.clear();
}

} //namespace
