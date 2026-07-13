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

namespace simu5g {

/**
 * NR-gNB MAC with D2D support.
 *
 * This class is currently empty: NrMacGnb still carries the D2D deltas
 * directly. Once NrMacGnb is re-parented onto LteMacEnb (dropping its
 * built-in D2D behavior), the NR-gNB D2D deltas move here.
 */
class NrMacGnbD2D : public NrMacGnb
{
};

} //namespace

#endif
