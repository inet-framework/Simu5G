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
 * MAC. All the shared D2D logic lives in the mixin (see D2dUeMacBase.h); this
 * leaf keeps only the LTE-specific main loop (synchronous H-ARQ scan with the
 * D2D direction filter and the DL corrupted-PDU purge).
 */
class LteMacUeD2D : public D2dUeMacBase<LteMacUe>
{
  protected:
    // signal registered here (not in the mixin or the helper) to keep the
    // global signal registration order -- and thus the sz fingerprint --
    // unchanged
    static simsignal_t rcvdD2DModeSwitchNotificationSignal_;

    /**
     * Main loop
     */
    void handleSelfMessage() override;

  public:
    LteMacUeD2D();
};

} //namespace

#endif
