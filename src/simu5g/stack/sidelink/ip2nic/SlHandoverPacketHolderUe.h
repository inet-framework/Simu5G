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

#ifndef _SIDELINK_SLHANDOVERPACKETHOLDERUE_H_
#define _SIDELINK_SLHANDOVERPACKETHOLDERUE_H_

#include "simu5g/stack/ip2nic/HandoverPacketHolderUe.h"

namespace simu5g {

class SlBinder;

/**
 * Handover packet holder for sidelink-capable UEs: PC5-destined packets are
 * deliverable without any serving node (they never traverse Uu).
 */
class SlHandoverPacketHolderUe : public HandoverPacketHolderUe
{
  protected:
    SlBinder *slBinder_ = nullptr;

    void initialize(int stage) override;
    bool isDeliverable(inet::Packet *datagram) override;
};

} // namespace simu5g

#endif
