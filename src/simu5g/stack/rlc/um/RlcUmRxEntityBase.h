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

#ifndef _SIMU5G_RLCUMRXENTITYBASE_H_
#define _SIMU5G_RLCUMRXENTITYBASE_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rlc/LteRlcDefs.h"

namespace simu5g {

using namespace omnetpp;

class LteMacBase;
class RlcMux;

/**
 * @class RlcUmRxEntityBase
 * @brief Common shell of the RLC UM receiving entity.
 *
 * Holds the MAC-mux plumbing, the D2D mode-switch entry point and the UL
 * data-burst throughput accounting (TS 136.314) shared by both RATs. The
 * buffering / reassembly / timer logic is deferred to the concrete subclasses:
 *  - LteRlcUmRxEntity (TS 36.322): FI-walk reassembly, PDU-SN reorder window.
 *  - NrRlcUmRxEntity  (TS 38.322): SI + byte-offset (SO) reassembly, SDU-SN window.
 *
 * Abstract base: not instantiated directly (no NED type, no Define_Module).
 */
class RlcUmRxEntityBase : public RlcRxEntityBase
{
  protected:

    // Node id of the owner module
    MacNodeId ownerNodeId_;

    // The mux feeding this entity (for UL burst-throughput reporting)
    RlcMux *rlcMux_ = nullptr;

    // After a D2D mode switch, the first PDU on the new-mode entity is forced in-sequence
    bool resetFlag_ = false;

    // UL data-burst accounting (TS 136.314), eNB only
    enum BurstCheck { ENQUE, REORDERING };
    bool isBurst_ = false;
    unsigned int totalBits_ = 0;
    unsigned int ttiBits_ = 0;
    simtime_t t1_ = 0;
    simtime_t t2_ = 0;

    // Signals. Declared/registered in one translation unit (RlcUmRxEntityBase.cc),
    // in their historical order, so registerSignal() ordering -- and therefore result
    // recording -- is unaffected by the split. Emitted by the concrete subclasses;
    // rlcCellThroughputSignal_ is shared (LTE emits it on the cell RLC, NR on itself).
    static simsignal_t rlcDelaySignal_[2];
    static simsignal_t rlcThroughputSignal_[2];
    static simsignal_t rlcPduDelaySignal_[2];
    static simsignal_t rlcPduThroughputSignal_[2];
    static simsignal_t rlcCellThroughputSignal_[2];
    static simsignal_t rlcDelayD2DSignal_;
    static simsignal_t rlcThroughputD2DSignal_;
    static simsignal_t rlcPduDelayD2DSignal_;
    static simsignal_t rlcPduThroughputD2DSignal_;
    static simsignal_t receivedPacketFromLowerLayerSignal_;
    static simsignal_t sentPacketToUpperLayerSignal_;

    void initialize(int stage) override;

    // UL throughput burst accounting (shared accumulator; per-mode buffer-empty check
    // via isEmpty()).
    void handleBurst(BurstCheck event);

    // --- mode-specific hooks, implemented by the concrete subclasses ---

    // Mode-specific INITSTAGE_LOCAL init (buffers, timers, window, rlcMux_ lookup).
    virtual void initMode(LteMacBase *mac) = 0;
    // Mode-specific INITSTAGE_SIMU5G_BINDER_ACCESS init (LTE binder/nodeB lookup).
    virtual void initBinderStage() {}

  public:

    // Enqueues a lower-layer PDU into the entity for reassembly
    virtual void enque(cPacket *pkt) = 0;

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    virtual void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer = true) = 0;

    // returns true if the entity contains no buffered RLC data
    virtual bool isEmpty() const = 0;
};

} //namespace

#endif
