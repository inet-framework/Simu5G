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
// NrMacUe.h (included below) already claims _NRMACUED2D_H_.
#ifndef _D2D_NRMACUED2D_H_
#define _D2D_NRMACUED2D_H_

#include "simu5g/stack/mac/NrMacUe.h"

namespace simu5g {

/**
 * NR-UE MAC with D2D support.
 *
 * This class is currently empty: NrMacUe still carries the D2D deltas
 * directly. Once NrMacUe is re-parented onto LteMacUe (dropping its
 * built-in D2D behavior), the NR-UE D2D deltas move here.
 */
class NrMacUeD2D : public NrMacUe
{
};

} //namespace

#endif
