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

#ifndef _QFI_RULE_SET_H_
#define _QFI_RULE_SET_H_

#include <memory>
#include <vector>

#include <inet/common/packet/PacketFilter.h>
#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/common/LteCommon.h"

namespace omnetpp { class cValueArray; }

namespace simu5g {

/**
 * @brief An ordered set of QFI-assignment rules, shared by every module that
 * classifies packets to QoS flows.
 *
 * The rule grammar is one and the same wherever a qfiRules parameter appears --
 * at the network side's tunnel entry (~TrafficFlowFilter, downlink) and at the
 * UE's stack entry (~QosFlowClassifier, uplink): a JSON array evaluated in
 * order, first match wins. Rule fields:
 *  - filter (string, optional): an inet::PacketFilter -- a message-name pattern
 *    (e.g. "*VoIP*") or an expression written as "expr(...)"
 *    (e.g. "expr(udp.destPort == 3000)"); omitted = the rule matches every packet
 *  - qfi (int, 0..63) or dscpAsQfi (bool): the QFI to assign -- a fixed value, or
 *    the packet's IPv4 DSCP field read as the QFI (exactly one of the two)
 *
 * A packet matching no rule is left unclassified: classify() returns QFI_NONE,
 * and what that means -- the default flow, or no marking at all -- is the
 * caller's to decide. (The distinction matters because QFI 0 is a real
 * classification, onto the default flow.)
 */
class QfiRuleSet
{
  protected:
    struct QfiRule {
        std::unique_ptr<inet::PacketFilter> filter;   // null = match all
        Qfi qfi = QFI_NONE;
        bool dscpAsQfi = false;
    };
    std::vector<QfiRule> rules_;

  public:
    // Parse and validate a qfiRules parameter value; errors name the parameter
    // and the entry, and throw at setup time rather than on the first packet.
    void parse(const omnetpp::cValueArray *rules, const char *paramName);

    // The QFI of the first matching rule, or QFI_NONE if no rule covers the packet
    Qfi classify(inet::Packet *pkt, const inet::Ptr<const inet::Ipv4Header>& ipv4Header) const;

    bool empty() const { return rules_.empty(); }
};

} // namespace simu5g

#endif
