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

#include "simu5g/stack/sdap/QosFlowClassifier.h"

#include <inet/networklayer/ipv4/Ipv4Header_m.h>

#include "simu5g/common/QfiTag_m.h"

namespace simu5g {

using namespace inet;

Define_Module(QosFlowClassifier);

void QosFlowClassifier::setQfiRules(QfiRuleSet&& rules)
{
    Enter_Method_Silent("setQfiRules");
    qfiRules_ = std::move(rules);
}

void QosFlowClassifier::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);

    // An already-present QFI (an application that set it directly) is not second-guessed.
    // A packet no rule covers stays untagged -- deliberately not tagged with 0, because
    // QFI 0 is a real classification (the default flow) and absence is what lets SDAP
    // fall back to reflective QoS. TODO IPv6
    if (!pkt->hasTag<QfiReq>() && !qfiRules_.empty()) {
        const auto& ipv4Header = pkt->peekAtFront<Ipv4Header>();
        Qfi qfi = qfiRules_.classify(pkt, ipv4Header);
        if (qfi != QFI_NONE) {
            pkt->addTag<QfiReq>()->setQfi(qfi);
            EV_INFO << "QosFlowClassifier - " << pkt->getName() << " classified to QFI " << qfi << "\n";
        }
        else
            EV_INFO << "QosFlowClassifier - " << pkt->getName() << " matches no rule, left unclassified\n";
    }

    send(pkt, "lowerLayerOut");
}

} // namespace simu5g
