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

#include "simu5g/common/QfiRuleSet.h"

namespace simu5g {

using namespace omnetpp;

void QfiRuleSet::parse(const cValueArray *rules, const char *paramName)
{
    for (int i = 0; i < (int)rules->size(); i++) {
        const cValueMap *entry = check_and_cast<const cValueMap *>(rules->get(i).objectValue());
        for (const auto& [key, value] : entry->getFields())
            if (key != "filter" && key != "qfi" && key != "dscpAsQfi")
                throw cRuntimeError("%s entry %d: unknown field '%s'", paramName, i, key.c_str());

        QfiRule rule;
        if (entry->containsKey("qfi")) {
            long qfi = entry->get("qfi").intValue();
            if (qfi < 0 || qfi > 63)
                throw cRuntimeError("%s entry %d: qfi %ld is outside the 0..63 range", paramName, i, qfi);
            rule.qfi = Qfi(qfi);
        }
        if (entry->containsKey("dscpAsQfi"))
            rule.dscpAsQfi = entry->get("dscpAsQfi").boolValue();
        if (entry->containsKey("qfi") == rule.dscpAsQfi)
            throw cRuntimeError("%s entry %d: exactly one of \"qfi\" and \"dscpAsQfi\" must be given", paramName, i);
        if (entry->containsKey("filter")) {
            rule.filter = std::make_unique<inet::PacketFilter>();
            configurePacketFilter(*rule.filter, entry->get("filter").stringValue());
        }
        rules_.push_back(std::move(rule));
    }
}

Qfi QfiRuleSet::classify(inet::Packet *pkt, const inet::Ptr<const inet::Ipv4Header>& ipv4Header) const
{
    for (const QfiRule& rule : rules_)
        if (rule.filter == nullptr || rule.filter->matches(pkt))
            return rule.dscpAsQfi ? Qfi(ipv4Header->getDscp()) : rule.qfi;
    return QFI_NONE;
}

} // namespace simu5g
