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

#ifndef _LTE_LTEMACENBD2D_H_
#define _LTE_LTEMACENBD2D_H_

#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/d2d/mac/D2dEnbMacBase.h"

namespace simu5g {

using namespace omnetpp;

/**
 * D2D-capable eNB MAC: the D2dEnbMacBase mixin layered over the core LTE MAC.
 * All the D2D logic lives in the mixin (see D2dEnbMacBase.h).
 */
class LteMacEnbD2D : public D2dEnbMacBase<LteMacEnb>
{
};

} //namespace

#endif
