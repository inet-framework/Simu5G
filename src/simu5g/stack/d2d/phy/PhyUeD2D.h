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

#ifndef _PHYUED2D_H_
#define _PHYUED2D_H_

#include "simu5g/stack/phy/PhyUe.h"
#include "simu5g/stack/d2d/phy/D2dUePhy.h"

namespace simu5g {

/**
 * D2D-capable LTE UE PHY: the D2dUePhy mixin layered over the core LTE UE PHY.
 * All the D2D logic lives in the mixin (see D2dUePhy.h).
 */
class PhyUeD2D : public D2dUePhy<PhyUe>
{
};

} //namespace

#endif /* _PHYUED2D_H_ */
