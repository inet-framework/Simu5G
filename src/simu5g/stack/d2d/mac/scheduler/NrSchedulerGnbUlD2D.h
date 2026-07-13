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

#ifndef _NRSCHEDULER_GNB_UL_D2D_H_
#define _NRSCHEDULER_GNB_UL_D2D_H_

#include "simu5g/stack/mac/scheduler/NrSchedulerGnbUl.h"
#include "simu5g/stack/d2d/mac/scheduler/D2dRtxScheduling.h"

namespace simu5g {

class ID2dMacEnb;

/**
 * @class NrSchedulerGnbUlD2D
 *
 * NR gNB uplink scheduler with device-to-device (D2D) support. Extends the clean
 * NrSchedulerGnbUl with the scheduling of D2D-mirror HARQ retransmissions and the
 * D2D frequency-reuse (ALLOCATOR_BESTFIT) discipline. It is the NR counterpart of
 * ~LteSchedulerEnbUlD2D; keep the two in sync.
 */
class NrSchedulerGnbUlD2D : public NrSchedulerGnbUl
{
  protected:
    // D2D facet of the owning MAC, resolved once at initialize()
    ID2dMacEnb *d2dMac_ = nullptr;

    // shared per-acid D2D retransmission logic (shared with the LTE variant)
    D2dRtxScheduling d2dRtxScheduling_;

    void initialize(int stage) override;

    /// Schedules the D2D-mirror HARQ retransmissions (NR asynchronous HARQ variant:
    /// iterates over all processes). Kept in sync with
    /// LteSchedulerEnbUlD2D::scheduleAdditionalRetransmissions() (synchronous HARQ:
    /// a single active acid per flow).
    unsigned int scheduleAdditionalRetransmissions(GHz carrierFrequency, BandLimitVector *bandLim = nullptr) override;

    /// The D2D uplink scheduler owns the ALLOCATOR_BESTFIT frequency-reuse discipline.
    LteScheduler *getScheduler(SchedDiscipline discipline) override;

  public:
    NrSchedulerGnbUlD2D() : d2dRtxScheduling_(this) {}
};

} //namespace

#endif // _NRSCHEDULER_GNB_UL_D2D_H_
