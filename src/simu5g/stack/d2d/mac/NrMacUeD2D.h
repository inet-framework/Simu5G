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

// NOTE: the header guard intentionally differs from the class name because
// NrMacUe.h (included below) already claims _NRMACUE_H_ (and historically
// _NRMACUED2D_H_).
#ifndef _D2D_NRMACUED2D_H_
#define _D2D_NRMACUED2D_H_

#include "simu5g/stack/mac/NrMacUe.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/ID2dMacUe.h"
#include "simu5g/stack/d2d/mac/D2dUeMacHelper.h"
#include "simu5g/stack/mac/buffer/harq_d2d/LteHarqBufferTxD2D.h"

namespace simu5g {

using namespace omnetpp;

/**
 * NR-UE MAC with D2D support.
 *
 * Mirrors ~LteMacUeD2D on top of the NR (numerology-aware) ~NrMacUe base:
 * the D2D-specific state and logic live in the shared D2dUeMacHelper, while
 * the numerology-aware handleSelfMessage()/macPduMake() carry the D2D deltas
 * that used to live directly in NrMacUe.
 */
class NrMacUeD2D : public NrMacUe, public ID2dMacUe
{

  protected:

    // holds the D2D-specific UE-MAC state and logic (shared with the LTE variant).
    // NOTE: the mode-switch-notification signal ID passed to the helper is interned
    // at RUNTIME in the constructor (not via a static registerSignal() in this TU):
    // the name is already registered by LteMacUeD2D.cc's static at its original
    // position, and a static in a new translation unit would perturb the global
    // signal registration order -- and thus the sz fingerprint -- link-order
    // dependently.
    D2dUeMacHelper d2dUeHelper_;

    /**
     * Reads MAC parameters for the UE and performs initialization.
     */
    void initialize(int stage) override;

    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    void handleMessage(cMessage *msg) override;

    /**
     * Main loop
     */
    void handleSelfMessage() override;

    void macHandleGrant(cPacket *pkt) override;

    /*
     * Checks RAC status
     */
    void checkRAC() override;

    /*
     * Receives and handles RAC responses
     */
    void macHandleRac(cPacket *pkt) override;

    void macHandleD2DModeSwitch(cPacket *pkt);

    /**
     * macPduMake() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List.
     * It sends them to H-ARQ (at the moment lower layer)
     *
     * On UE it also adds a BSR control element to the MAC PDU
     * containing the size of its buffer (for that CID)
     */
    void macPduMake(MacCid cid = MacCid()) override;

    /// HARQ buffer factories: add support for the D2D and D2D_MULTI directions
    LteHarqBufferRx *createRxHarqBuffer(MacNodeId src, const UserControlInfo *userInfo) override;
    LteHarqBufferTx *createTxHarqBuffer(MacNodeId destId, Direction dir) override;

    /// Factory override: use the D2D-capable LCG scheduler
    LcgScheduler *createLcgScheduler() override;

  public:
    NrMacUeD2D();

    void doHandover(MacNodeId targetEnb) override;
};

} //namespace

#endif
