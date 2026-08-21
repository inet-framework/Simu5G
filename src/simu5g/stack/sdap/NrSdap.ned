//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sdap;

//
// Implements the SDAP Protocol.
//
// For PDUs received from the PDCP layer, extracts SDAP headers (if present),
// restores the associated QoS Flow based on QFI, and forwards the resulting SDUs
// to the upper layer (IP, Ethernet, or application for unstructured sessions).
//
// For SDUs received from the upper layer, performs QFI-to-DRB mapping,
// encapsulates the data with SDAP headers if needed, and forwards the packets
// to the PDCP layer. On the gNB DL path, the QFI is always taken from the
// QfiReq tag (set by GtpUser). On the UE UL path two sources may answer: the
// classified QFI (the QfiReq tag, stamped by ~QosFlowClassifier's rules or set
// by the application directly) and a reflective QoS match, the rule derived
// from observed downlink traffic. When both answer, reflectiveQosOverridesQfi
// says which wins; one alone is used as-is; a packet with neither joins the
// default QoS flow (QFI 0, the default DRB).
//
// An SDAP header is only added when the QFI cannot be unambiguously recovered
// from the DRB alone on the RX side, i.e. when the DRB is the default DRB
// (which may carry unmapped fallback traffic) or when it has multiple QFIs
// mapped to it.
//
// Supports all 3GPP PDU session types (IPv4, IPv6, Ethernet, Unstructured)
// via the pduSessionType field of the bearer configuration.
//
// SDAP does not author its own configuration: the QFI-to-DRB mapping and the
// per-bearer SDAP settings are authored network-wide (see the staticDrbs
// parameter of ~Smf), delivered to RRC, and pushed into this module from
// there at initialization.
//
// Gates:
//  - upperLayerIn/upperLayerOut: connects to the upper layer (Ip2Nic or app).
//  - pdcpIn/pdcpOut: connects to the PDCP layer.
//
simple NrSdap like INrSdap
{
    parameters:
        string nodeRole @enum("UE", "gNB");  // Node role: "UE" or "gNB" - determines reflective QoS behavior
        string smfModule = default("smf");
        bool establishBearersOnDemand = default(true);  // when false, a packet that finds no established bearer raises an error instead of establishing one (static bearers are established up front; see the staticDrbs parameter of ~Smf)
        string reflectiveQosTableModule = default("");  // UE only: path to ReflectiveQosTable module (empty = disabled)
        bool useReflectiveQos = default(false); // UE only
        bool reflectiveQosOverridesQfi = default(false);  // UE only: when a packet has both a classified QFI (QfiReq tag) and a reflective QoS match, true = the reflective QFI wins, false = the classified one; a packet with only one of the two always uses it
        @display("i=block/layer");
    gates:
        input upperLayerIn;     // from upper layer (Ip2Nic or app)
        output upperLayerOut;   // to upper layer (Ip2Nic or app)
        input pdcpIn;           // from PDCP layer
        output pdcpOut;         // to PDCP layer
}
