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

#ifndef _LTE_LTECONTROLINFO_H_
#define _LTE_LTECONTROLINFO_H_

#include "simu5g/common/LteControlInfo_m.h"

namespace simu5g {
inline void markUpstack(inet::Packet *pkt) {
    EV_STATICCONTEXT;
    EV << "markUpstack " << pkt->getName() << std::endl;
    auto& uci = pkt->addTagIfAbsent<UserControlInfo>();
    auto& fci = pkt->addTagIfAbsent<FlowControlInfo>();
    if (uci->isDownStack())
        throw cRuntimeError(pkt, "markUpstack: Already marked DownStack in uci!");
    if (fci->isDownStack())
        throw cRuntimeError(pkt, "marUpstack: Already marked DownStack in fci!");
    uci->setIsUpStack(true);
    fci->setIsUpStack(true);
}

inline void markDownstack(inet::Packet *pkt) {
    EV_STATICCONTEXT;
    EV << "markDownstack " << pkt->getName() << std::endl;
    auto& uci = pkt->addTagIfAbsent<UserControlInfo>();
    auto& fci = pkt->addTagIfAbsent<FlowControlInfo>();
    if (uci->isUpStack())
        throw cRuntimeError(pkt, "markDownstack: Already marked upstack in uci!");
    if (fci->isUpStack())
        throw cRuntimeError(pkt, "markDownstack: Already marked upstack in fci!");
    uci->setIsDownStack(true);
    fci->setIsDownStack(true);
}

inline void cleanDirectionMark(inet::Packet *pkt) {
    EV_STATICCONTEXT;
    EV << "clearDirectionMark " << pkt->getName() << std::endl;
    pkt->addTagIfAbsent<UserControlInfo>()->setIsUpStack(false);
    pkt->addTagIfAbsent<FlowControlInfo>()->setIsUpStack(false);
    pkt->addTagIfAbsent<UserControlInfo>()->setIsDownStack(false);
    pkt->addTagIfAbsent<FlowControlInfo>()->setIsDownStack(false);
}

} // namespace

#endif

