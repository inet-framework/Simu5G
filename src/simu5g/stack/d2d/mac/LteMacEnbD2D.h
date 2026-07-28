//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_LTEMACENBD2D_H_
#define _LTE_LTEMACENBD2D_H_

#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/d2d/mac/D2dEnbMacBase.h"

namespace simu5g {

using namespace omnetpp;

/**
 * D2D-capable eNB MAC: the D2dEnbMacBase mixin layered over the core LTE MAC.
 * All the D2D logic lives in the mixin (see D2dEnbMacBase.h).
 *
 * The two overrides below are NOT D2D-specific: they are the historical
 * "fork" variants of sendGrants/macPduUnmake that the D2D (and NR) MACs
 * follow, while LteMacEnb keeps the plain-LTE originals (grant chunk length
 * 1 bit vs 1 byte, BSR CE keyed by the packet LCID vs LogicalCid(0)). They
 * are byte-identical to NrMacGnb's copies; reconciling all of them onto
 * hook-parameterized LteMacEnb bodies is a separate, fingerprint-affecting
 * step.
 */
class LteMacEnbD2D : public D2dEnbMacBase<LteMacEnb>
{
  protected:
    /**
     * macPduUnmake() extracts SDUs from a received MAC
     * PDU and sends them to the upper layer.
     *
     * On ENB it also extracts the BSR Control Element
     * and stores it in the BSR buffer (for the cid from
     * which the packet was received)
     *
     * @param pkt container packet
     */
    void macPduUnmake(cPacket *pkt) override;

    /**
     * creates scheduling grants (one for each nodeId) according to the Schedule List.
     * It sends them to the lower layer
     */
    void sendGrants(std::map<GHz, LteMacScheduleList> *scheduleList) override;
};

} //namespace

#endif
