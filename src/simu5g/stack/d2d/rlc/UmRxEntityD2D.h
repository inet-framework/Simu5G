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

#ifndef _LTE_UMRXENTITY_D2D_H_
#define _LTE_UMRXENTITY_D2D_H_

#include "simu5g/stack/rlc/um/UmRxEntity.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @class UmRxEntityD2D
 * @brief D2D-aware receiver entity for UM.
 *
 * Extends ~UmRxEntity with the machinery needed by D2D:
 * - D2D_MULTI connections initialize the reordering window on the first
 *   received PDU and use a single-PDU window (no reordering for D2D
 *   multicast);
 * - after a D2D mode switch, the sequence numbering is reset and the next
 *   PDU is treated as in-sequence;
 * - throughput/delay statistics are emitted on the D2D signals for
 *   D2D/D2D_MULTI flows.
 * Created by BearerManagement on the D2D-capable NICs.
 */
class UmRxEntityD2D : public UmRxEntity
{
  protected:

    bool init_ = false;

    // If true, the next PDU and the corresponding SDUs are considered in order
    // (modify the lastPduReassembled_ counter)
    // useful for D2D after a mode switch
    bool resetFlag_ = false;

    // D2D_MULTI: the first received PDU initializes a single-PDU window
    void onFirstPduEnqueued(unsigned int pduSno) override;

    // consumes a reassembly reset pending after a D2D mode switch
    bool consumeReassemblyReset(unsigned int pduSno) override;

    // D2D arms of the per-PDU/per-SDU statistics
    void emitPduStats(cModule *ue, Direction dir, double tputSample, simtime_t creationTime) override;
    void emitSduStats(cModule *ue, Direction dir, double tputSample, simtime_t creationTime) override;

  public:

    // returns true if this entity is for a D2D_MULTI connection
    bool isD2DMultiConnection() { return flowControlInfo_->getDirection() == D2D_MULTI; }

    // called when a D2D mode switch is triggered
    void rlcHandleD2DModeSwitch(bool oldConnection, bool oldMode, bool clearBuffer = true);
};

} //namespace

#endif
