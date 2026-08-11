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

#ifndef _NRAMCD2D_H_
#define _NRAMCD2D_H_

#include "simu5g/stack/mac/amc/NrAmc.h"
#include "simu5g/stack/d2d/mac/amc/D2dAmc.h"

namespace simu5g {

/**
 * NR AMC with device-to-device (D2D) support: the D2dAmc mixin layered over
 * the clean NrAmc. All the shared D2D logic lives in the mixin (see D2dAmc.h);
 * the only NR-specific addition is the D2D branch of the NR MCS table seam.
 */
class NrAmcD2D : public D2dAmc<NrAmc>
{
  protected:
    NrMcsTable *getNrMcsTableForDirection(Direction dir) override;
};

} //namespace

#endif
