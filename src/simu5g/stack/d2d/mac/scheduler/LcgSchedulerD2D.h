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

#ifndef _LTE_LCGSCHEDULERD2D_H_
#define _LTE_LCGSCHEDULERD2D_H_

#include "simu5g/stack/mac/scheduler/LcgScheduler.h"

namespace simu5g {

/**
 * @class LcgSchedulerD2D
 *
 * LCG (Logical Channel Group) UE uplink scheduler with device-to-device (D2D)
 * support. Extends the clean LcgScheduler with the workaround that reserves an UL
 * grant for the BSR of an active D2D connection. Created by the D2D UE MACs
 * (LteMacUeD2D, NrMacUeD2D) via createLcgScheduler().
 */
class LcgSchedulerD2D : public LcgScheduler
{
  public:
    LcgSchedulerD2D(LteMacUe *mac) : LcgScheduler(mac) {}

  protected:
    bool checkForPendingAdditionalBsr(Direction grantDir, LteTrafficClass tc) override;
};

} //namespace

#endif // _LTE_LCGSCHEDULERD2D_H_
