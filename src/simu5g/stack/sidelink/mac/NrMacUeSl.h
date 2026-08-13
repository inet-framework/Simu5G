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

#ifndef _SIDELINK_NRMACUESL_H_
#define _SIDELINK_NRMACUESL_H_

#include "simu5g/stack/mac/NrMacUe.h"

namespace simu5g {

class NrSlMacUe;

/**
 * SL-aware NR Uu MAC for the UE (design decisions D26/D27/D28, SL-3): the
 * Uu side of the mode-1 request loop. The SL MAC (NrSlMacUe) deliberately
 * has no RAC/BSR plumbing (G19); this subclass bridges the two stacks via
 * direct C++ calls (the established onSciDecoded-style pattern):
 *
 *  - checkRAC(): after the base's Uu-buffer scan, an SL-only backlog
 *    (slMac->mode1BsrPending()) triggers the same preamble RAC; a pending
 *    Uu request always wins ties (SL retries next cycle via bsrRtxTimer)
 *  - macHandleRac(): a won RAC that was SL-triggered arms slBsrTriggered_
 *  - macPduMake(): with an SL-BSR pending, a UL grant and an empty UL
 *    schedule list, a BSR-only PDU is emitted whose
 *    UserControlInfo::packetLcid = SL_SHORT_BSR (the D2D BSR-only-PDU
 *    pattern); otherwise the base makes PDUs as usual
 *  - macHandleGrant(): an arriving SlSchedulingGrant (D28) is routed to
 *    slMac->onMode1Grant() and never touches base Uu grant state (G22);
 *    all other grants go to the base
 *
 * Selected by the hasSidelink NED default of the nrMac slot; with no
 * mode-1 connections every override is quiescent and the class behaves
 * byte-identically to NrMacUe (the WP-O quiescence gate).
 */
class NrMacUeSl : public NrMacUe
{
  protected:
    NrSlMacUe *slMac_ = nullptr;

    bool slRacRequested_ = false;   // the in-flight RAC was SL-triggered
    bool slBsrTriggered_ = false;   // a won SL RAC armed the SL-BSR

    void initialize(int stage) override;

    void checkRAC() override;
    void macHandleRac(cPacket *pktAux) override;
    void macHandleGrant(cPacket *pktAux) override;
    void macPduMake(MacCid cid = MacCid()) override;

    /// widened BSR predicate: the BSR-opportunity gate in the base main
    /// loop fires for the SL-BSR too (D26)
    bool isBsrPending() const override { return bsrTriggered_ || slBsrTriggered_; }

    /// build and enqueue the SL-BSR-only PDU on the given carrier's grant;
    /// returns false when the SL backlog vanished (trigger cleared)
    bool makeSlBsrPdu(GHz carrierFreq);
};

} // namespace simu5g

#endif
