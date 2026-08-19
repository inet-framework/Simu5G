//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

package simu5g.stack.sdap;

//
// Classifies a UE's uplink packets to QoS flows: the uplink counterpart of
// ~TrafficFlowFilter's QFI assignment, sitting where uplink traffic enters the
// UE's stack just as the TFF sits where downlink traffic enters the tunnel at
// the core network. Each direction is classified once, at its ingress.
//
// The qfiRules parameter is the same rule grammar the TFF's parameter of the
// same name uses: a JSON array evaluated in order, first match wins. Rule fields:
//  - filter (string, optional): an inet::PacketFilter -- a message-name pattern
//    (e.g. "*VoIP*") or an expression written as "expr(...)"
//    (e.g. "expr(udp.destPort == 3000)"); omitted = the rule matches every packet
//  - qfi (int, 0..63) or dscpAsQfi (bool): the QFI to assign -- a fixed value, or
//    the packet's IPv4 DSCP field read as the QFI (exactly one of the two)
//
// A matched packet is stamped with a QfiReq tag, which ~NrSdap consumes for its
// QFI-to-DRB mapping. A packet no rule covers is left untagged -- deliberately
// not tagged with 0, because QFI 0 is a real classification (the default flow)
// and absence is what lets SDAP fall back to reflective QoS or the default DRB.
// A packet that already carries a QfiReq (an application that set the QFI
// directly) is not second-guessed. The default, [], classifies nothing: SDAP
// behaves as if this module were not there.
//
// The rules model the QoS rules that NAS signaling installs into a UE at PDU
// session establishment (TS 23.501 5.7.1.4). No such signaling exists in the
// model, so a locally configured rule table is a modeling shortcut: the ini
// author plays the NAS, exactly as they play the SMF at the ~TrafficFlowFilter.
//
simple QosFlowClassifier like IQosFlowClassifier
{
    parameters:
        object qfiRules = default([]);  // JSON array of QFI-assignment rules [{filter, qfi|dscpAsQfi}, ...], first match wins; [] = classify nothing; see banner comment
        @display("i=block/classifier");
    gates:
        input upperLayerIn;
        output lowerLayerOut;
}
