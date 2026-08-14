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

#ifndef _SIDELINK_NRMACGNBSL_H_
#define _SIDELINK_NRMACGNBSL_H_

#include "simu5g/stack/mac/NrMacGnb.h"
#include "simu5g/stack/sidelink/mac/SlEnbScheduler.h"

namespace simu5g {

class SlGnbRrc;

/**
 * SL-aware NR gNB MAC (design decision D27, SL-3): the gNB side of the
 * mode-1 request loop. Owns nothing itself - it intercepts SL-BSR PDUs
 * (UserControlInfo::packetLcid == SL_SHORT_BSR) in macPduUnmake() BEFORE
 * they could reach bufferizeBsr()/the Uu UL scheduler (G21), hands them to
 * SlGnbRrc -> SlEnbScheduler, and sends the resulting SlSchedulingGrant
 * (D28) back on the stock GRANTPKT path (the sendGrants packet-construction
 * pattern, seam 2).
 *
 * Selected by NrNicEnbSl's mac slot default; without sidelink UEs the
 * override is quiescent (no PDU ever carries SL_SHORT_BSR).
 */
class NrMacGnbSl : public NrMacGnb
{
  protected:
    SlGnbRrc *slGnbRrc_ = nullptr;

    void initialize(int stage) override;

    void macPduUnmake(cPacket *pkt) override;

    /// build the DCI 3_0-equivalent and send it over the Uu DL (seam 2)
    virtual void sendSlGrant(MacNodeId ueId, const SlEnbScheduler::GrantSpec& spec, GHz carrierFreq);
};

} // namespace simu5g

#endif
