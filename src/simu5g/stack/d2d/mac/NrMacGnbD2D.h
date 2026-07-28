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

#ifndef _NRMACGNBD2D_H_
#define _NRMACGNBD2D_H_

#include "simu5g/stack/mac/NrMacGnb.h"
#include "simu5g/stack/d2d/mac/D2dEnbMacBase.h"

namespace simu5g {

/**
 * D2D-capable gNB MAC: the D2dEnbMacBase mixin layered over the clean NR MAC.
 * All the D2D logic lives in the mixin (see D2dEnbMacBase.h); NrMacGnb already
 * provides the sendGrants/macPduUnmake variants the D2D MACs need, so no
 * further overrides are necessary here.
 */
class NrMacGnbD2D : public D2dEnbMacBase<NrMacGnb>
{
};

} //namespace

#endif
