//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _NRMACGNB_H_
#define _NRMACGNB_H_

#include "simu5g/stack/mac/LteMacEnb.h"

namespace simu5g {

class NrMacGnb : public LteMacEnb
{
  public:

    NrMacGnb();

  protected:
    /**
     * macPduUnmake() extracts SDUs from a received MAC PDU and sends them to
     * the upper layer; it also extracts BSR control elements into the BSR buffer.
     *
     * NOTE: this override preserves the pre-separation NR gNB behavior, which
     * followed the LteMacEnbD2D fork rather than LteMacEnb (BSR keyed by the
     * packet LCID instead of LogicalCid(0)). Defect logged; the LteMacEnb /
     * LteMacEnbD2D fork is to be reconciled explicitly later.
     */
    void macPduUnmake(cPacket *pkt) override;

    /**
     * Creates scheduling grants (one for each nodeId) according to the
     * schedule list and sends them to the lower layer.
     *
     * NOTE: this override preserves the pre-separation NR gNB behavior, which
     * followed the LteMacEnbD2D fork rather than LteMacEnb (grant chunk length
     * set to b(1) instead of the 1-byte message default; grant direction from
     * the BSR LCID). Defect logged; to be reconciled explicitly later.
     */
    void sendGrants(std::map<GHz, LteMacScheduleList> *scheduleList) override;
};

} //namespace

#endif

