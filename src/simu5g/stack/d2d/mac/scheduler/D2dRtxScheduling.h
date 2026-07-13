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

#ifndef _LTE_D2DRTXSCHEDULING_H_
#define _LTE_D2DRTXSCHEDULING_H_

#include "simu5g/common/LteCommon.h"

namespace simu5g {

class LteSchedulerEnbUl;

/*
 * Plain helper that holds the D2D uplink retransmission logic shared by every
 * D2D-capable eNB/gNB uplink scheduler (LteSchedulerEnbUlD2D and
 * NrSchedulerGnbUlD2D). It is owned by value by the D2D uplink scheduler and
 * reaches into the owning scheduler's state through friendship (mirroring the
 * existing scheduling-module friends of LteSchedulerEnb), so that an uplink
 * scheduler that does NOT derive from LteSchedulerEnbUlD2D (i.e.
 * NrSchedulerGnbUlD2D) can reuse it.
 *
 * Mirrors the D2dAmcHelper pattern used for the AMC split: the per-acid D2D
 * retransmission body is identical between the LTE and NR uplink schedulers and
 * lives here; only the outer per-flow retransmission loop (which differs between
 * synchronous LTE and asynchronous NR HARQ) stays in each subclass.
 */
class D2dRtxScheduling
{
  protected:
    // the D2D uplink scheduler owning this helper
    LteSchedulerEnbUl *scheduler_;

  public:
    D2dRtxScheduling(LteSchedulerEnbUl *scheduler) : scheduler_(scheduler) {}

    /**
     * Schedules a retransmission for a single acid/codeword of a D2D-mirror HARQ
     * process, on a set of logical bands. Shared, identical between the LTE and
     * NR D2D uplink schedulers.
     *
     * @return The allocated bytes. 0 if retransmission was not possible.
     */
    unsigned int schedulePerAcidRtxD2D(MacNodeId destId, MacNodeId senderId, GHz carrierFrequency, Codeword cw, unsigned char acid,
            std::vector<BandLimit> *bandLim = nullptr, Remote antenna = MACRO, bool limitBl = false);
};

} //namespace

#endif // _LTE_D2DRTXSCHEDULING_H_
