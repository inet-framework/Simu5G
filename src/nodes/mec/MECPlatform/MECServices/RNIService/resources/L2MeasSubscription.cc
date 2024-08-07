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

#include "common/utils/utils.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/L2MeasSubscription.h"
#include <iostream>

namespace simu5g {

using namespace omnetpp;

L2MeasSubscription::L2MeasSubscription(unsigned int subId, inet::TcpSocket *socket, const std::string& baseResLocation,
        std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs) :
    SubscriptionBase(subId, socket, baseResLocation, eNodeBs) {}

void L2MeasSubscription::sendSubscriptionResponse() {
    nlohmann::ordered_json val;
    val[subscriptionType_]["callbackReference"] = callbackReference_;
    val[subscriptionType_]["_links"]["self"] = links_;
    val[subscriptionType_]["filterCriteria"] = filterCriteria_.associteId_.toJson();
    val[subscriptionType_]["filterCriteria"] = filterCriteria_.ecgi.toJson();
}

void L2MeasSubscription::sendNotification(EventNotification *event) {}

} //namespace

