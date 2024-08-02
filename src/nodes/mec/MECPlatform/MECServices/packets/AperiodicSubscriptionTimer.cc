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

#include "AperiodicSubscriptionTimer.h"

namespace simu5g {

AperiodicSubscriptionTimer::AperiodicSubscriptionTimer() : AperiodicSubscriptionTimer_m() {
    // TODO Auto-generated constructor stub
}

AperiodicSubscriptionTimer::AperiodicSubscriptionTimer(const char *name, const double& period):AperiodicSubscriptionTimer_m(name), subIdSet_()
{
    setPeriod(period);
}

AperiodicSubscriptionTimer::AperiodicSubscriptionTimer(const char *name) : AperiodicSubscriptionTimer_m(name), subIdSet_() {}

AperiodicSubscriptionTimer::~AperiodicSubscriptionTimer() {
    // TODO Auto-generated destructor stub
}

} //namespace

