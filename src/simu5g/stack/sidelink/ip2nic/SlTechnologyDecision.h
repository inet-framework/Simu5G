//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIDELINK_SLTECHNOLOGYDECISION_H_
#define _SIDELINK_SLTECHNOLOGYDECISION_H_

#include "simu5g/stack/ip2nic/TechnologyDecision.h"

namespace simu5g {

class SlBinder;

/**
 * Technology decision for sidelink-capable UEs (fixes gap G1): packets whose
 * destination is a registered PC5 destination bypass the "not attached to any
 * serving node" drop and are handed down tagged for the NR (SL) leg; all
 * other traffic gets the base behavior. From SL-2 on the same bypass applies
 * to unicast destinations that are SL-capable peers (the D16 static rule,
 * mirroring SlIp2Nic's classification).
 */
class SlTechnologyDecision : public TechnologyDecision
{
  protected:
    SlBinder *slBinder_ = nullptr;
    bool pc5UnicastEnabled_ = false;

    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;
};

} // namespace simu5g

#endif
