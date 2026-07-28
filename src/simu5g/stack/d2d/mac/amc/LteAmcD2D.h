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

#ifndef _LTE_LTEAMCD2D_H_
#define _LTE_LTEAMCD2D_H_

#include "simu5g/stack/mac/amc/LteAmc.h"
#include "simu5g/stack/d2d/mac/amc/AmcD2D.h"

namespace simu5g {

/**
 * LTE AMC with device-to-device (D2D) support: the AmcD2D mixin layered over
 * the clean LteAmc. All the D2D logic lives in the mixin (see AmcD2D.h).
 */
class LteAmcD2D : public AmcD2D<LteAmc>
{
};

} //namespace

#endif
