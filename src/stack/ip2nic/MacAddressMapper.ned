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

package simu5g.stack.ip2nic;

//
// The MacAddressMapper module enables integration of LTE/5G MAC layers as ports
// into Ethernet switch infrastructure. It acts as a protocol translator between
// IEEE 802 Ethernet and LTE/5G cellular domains, allowing cellular base stations
// to appear as Ethernet switch ports in converged networking scenarios.
//
// This module bridges the gap between traditional Ethernet switching and cellular
// networking by providing bidirectional MAC address translation:
//
// Ethernet → Cellular (Downstream):
// - Receives Ethernet frames with IEEE 802 MAC destination addresses
// - Extracts the destination MAC address from IEEE 802 request tags
// - Maps the lower 16 bits of the MAC address to LTE destId for cellular routing
// - Forwards packets to the cellular stack (PDCP-RRC) with proper LTE control info
//
// Cellular → Ethernet (Upstream):
// - Receives packets from the cellular stack with LTE control information
// - Generates IEEE 802 MAC address indications for Ethernet switching
// - Creates source MAC from cellular srcId (16 bits) + hardcoded prefix (0x020000000000)
// - Resolves destination IPv4 to MAC via ARP on all broadcast interfaces
// - Adds MacAddressInd tags so packets can be properly switched in Ethernet infrastructure
// - Forwards packets to the Ethernet domain
//
// Converged Network Integration:
// - Enables seamless packet flow between Ethernet switches and cellular infrastructure
// - Supports ARP resolution across the converged network topology
// - Maintains packet queues during ARP resolution to ensure reliable delivery
// - Provides transparent integration of cellular MACs as Ethernet switch ports
//
// ARP Integration for Converged Networks:
// - Uses INET's ARP module for cross-domain MAC address resolution
// - Starts ARP resolution simultaneously on all broadcast interfaces
// - Handles ARP completion/failure signals for robust network operation
// - Queues packets during pending ARP resolutions
// - Supports discovery of devices across Ethernet and cellular domains
//
simple MacAddressMapper
{
    parameters:
        string arpModule = default("^.arp");                    // ARP module path
        string interfaceTableModule = default("^.interfaceTable"); // Interface table module path
        @display("i=block/layer");
    gates:
        input upperLayerIn;   // from IP2Nic
        output upperLayerOut; // to IP2Nic
        input lowerLayerIn;   // from PDCP-RRC
        output lowerLayerOut; // to PDCP-RRC
}
