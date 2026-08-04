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
#include "simu5g/stack/d2d/mac/D2dUeMacBase.h"

namespace simu5g {

using namespace omnetpp;

/**
 * D2D-capable LTE UE MAC: the D2dUeMacBase mixin layered over the core LTE UE
 * MAC. All the shared D2D logic lives in the mixin (see D2dUeMacBase.h); the
 * main loop is inherited from LteMacUe, with the D2D deltas flowing through
 * the isBsrPending() (mixin) and purgeRxHarqBuffers() seams.
 */
class LteMacUeD2D : public D2dUeMacBase<LteMacUe>
{
  protected:
    /// purge corrupted PDUs of the DL buffer only, keeping the D2D mirror
    /// buffers intact
    void purgeRxHarqBuffers() override;
};

} //namespace

#endif
