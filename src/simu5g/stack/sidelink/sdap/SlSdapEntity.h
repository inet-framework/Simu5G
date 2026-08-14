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

#ifndef _SIDELINK_SLSDAPENTITY_H_
#define _SIDELINK_SLSDAPENTITY_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"
#include "simu5g/stack/sidelink/common/SlPreconfig.h"

namespace simu5g {

class SlRrc;
class SlBinder;

/**
 * Per-destination sidelink SDAP entity (design decision D20): PFI -> SLRB
 * mapping on TX, QfiInd stamping on RX. See SlSdapEntity.ned.
 */
class SlSdapEntity : public omnetpp::cSimpleModule
{
  protected:
    bool isTx_ = false;
    uint32_t peerKey_ = 0;        // TX: destination L2 ID; RX: source pid
    SlRrc *slRrc_ = nullptr;
    SlBinder *slBinder_ = nullptr;

    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;

    /// the SLRBs toward/from this entity's peer (link SLRBs for unicast,
    /// the preconfig's slrbConfig slice otherwise)
    std::vector<const SlrbConfigEntry *> getSlrbCandidates() const;

    void handleTxPacket(inet::Packet *pkt);
    void handleRxPacket(inet::Packet *pkt);
};

} // namespace simu5g

#endif
