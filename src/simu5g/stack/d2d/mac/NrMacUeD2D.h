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
#ifndef _NRMACUED2D_H_
#define _NRMACUED2D_H_

#include "simu5g/stack/mac/NrMacUe.h"
#include "simu5g/stack/d2d/mac/D2dUeMacBase.h"

namespace simu5g {

/**
 * D2D-capable NR UE MAC: the D2dUeMacBase mixin layered over the clean NR UE
 * MAC. All the D2D logic lives in the mixin (see D2dUeMacBase.h); the NR main
 * loop is inherited from NrMacUe (its only D2D delta flows through the
 * isBsrPending() override in the mixin).
 *
 * NOTE: the mode-switch signal name is interned at RUNTIME in the constructor
 * (not via a static registerSignal() in this TU): the name is already
 * registered by LteMacUeD2D.cc's static at its original position, and a static
 * in a new translation unit would perturb the global signal registration
 * order -- and thus the sz fingerprint -- link-order dependently.
 */
class NrMacUeD2D : public D2dUeMacBase<NrMacUe>
{
  public:
    NrMacUeD2D();
};

} //namespace

#endif
