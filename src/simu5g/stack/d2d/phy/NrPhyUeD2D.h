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

#ifndef _NRPHYUED2D_H_
#define _NRPHYUED2D_H_

#include "simu5g/stack/phy/NrPhyUe.h"
#include "simu5g/stack/d2d/phy/D2dUePhy.h"

namespace simu5g {

/**
 * D2D-capable NR UE PHY: the D2dUePhy mixin layered over the clean NR UE PHY.
 * All the D2D logic lives in the mixin (see D2dUePhy.h). Must remain an is-a
 * NrPhyUe: the dynamic_cast<NrPhyUe *> in HandoverController is the
 * dual-stack discriminator.
 */
class NrPhyUeD2D : public D2dUePhy<NrPhyUe>
{
};

} //namespace

#endif
