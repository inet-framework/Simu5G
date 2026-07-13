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

namespace simu5g {

/**
 * NR-UE PHY with D2D support.
 *
 * This class is currently empty: NrPhyUe still carries the D2D deltas
 * directly. Once NrPhyUe is re-parented onto LtePhyUe (dropping its
 * built-in D2D behavior), the NR-UE PHY D2D deltas move here.
 */
class NrPhyUeD2D : public NrPhyUe
{
};

} //namespace

#endif /* _NRPHYUED2D_H_ */
