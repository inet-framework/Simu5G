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

#ifndef _LTE_LTEMACUED2D_H_
#define _LTE_LTEMACUED2D_H_

#include "simu5g/stack/mac/LteMacUe.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/ID2dMacUe.h"
#include "simu5g/stack/d2d/mac/D2dUeMacHelper.h"
#include "simu5g/stack/d2d/mac/harq/LteHarqBufferTxD2D.h"

namespace simu5g {

using namespace omnetpp;

class LteSchedulingGrant;
class LteSchedulerUeUl;
class Binder;

class LteMacUeD2D : public LteMacUe, public ID2dMacUe
{

  protected:

    // signal registered here (not in the helper) to keep the global signal
    // registration order -- and thus the sz fingerprint -- unchanged
    static simsignal_t rcvdD2DModeSwitchNotificationSignal_;

    // holds the D2D-specific UE-MAC state and logic (shared with the NR variant)
    D2dUeMacHelper d2dUeHelper_;

    // flag for empty schedule list (true when no carriers have been scheduled)
    bool emptyScheduleList_;

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

    virtual void macHandleD2DModeSwitch(cPacket *pkt);

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
    LteMacUeD2D();

    virtual void triggerBsr(MacCid cid)
    {
        if (connDescOut_[cid].flowInfo.getDirection() == D2D_MULTI)
            d2dUeHelper_.setBsrD2DMulticastTriggered(true);
        else
            bsrTriggered_ = true;
    }

    void doHandover(MacNodeId targetEnb) override;
};

} //namespace

#endif
