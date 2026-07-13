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

#ifndef _LTE_LTESCHEDULERENBULD2D_H_
#define _LTE_LTESCHEDULERENBULD2D_H_

#include "simu5g/stack/mac/scheduler/LteSchedulerEnbUl.h"
#include "simu5g/stack/d2d/mac/scheduler/D2dRtxScheduling.h"

namespace simu5g {

class ID2dMacEnb;

/**
 * @class LteSchedulerEnbUlD2D
 *
 * LTE eNB uplink scheduler with device-to-device (D2D) support. Extends the
 * clean LteSchedulerEnbUl with the scheduling of D2D-mirror HARQ retransmissions
 * and the D2D frequency-reuse (ALLOCATOR_BESTFIT) discipline. It is the LTE
 * counterpart of ~NrSchedulerGnbUlD2D; keep the two in sync.
 */
class LteSchedulerEnbUlD2D : public LteSchedulerEnbUl
{
  protected:
    // D2D facet of the owning MAC, resolved once at initialize()
    ID2dMacEnb *d2dMac_ = nullptr;

    // shared per-acid D2D retransmission logic (shared with the NR variant)
    D2dRtxScheduling d2dRtxScheduling_;

    void initialize(int stage) override;

    /// Schedules the D2D-mirror HARQ retransmissions (LTE synchronous HARQ variant:
    /// a single active acid per flow). Kept in sync with
    /// NrSchedulerGnbUlD2D::scheduleAdditionalRetransmissions() (asynchronous HARQ:
    /// iterates over all processes).
    unsigned int scheduleAdditionalRetransmissions(GHz carrierFrequency, BandLimitVector *bandLim = nullptr) override;

    /// The D2D uplink scheduler owns the ALLOCATOR_BESTFIT frequency-reuse discipline.
    LteScheduler *getScheduler(SchedDiscipline discipline) override;

  public:
    LteSchedulerEnbUlD2D() : d2dRtxScheduling_(this) {}
};

} //namespace

#endif // _LTE_LTESCHEDULERENBULD2D_H_
