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

#ifndef _NRPHYUE_H_
#define _NRPHYUE_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/stack/phy/LtePhyUe.h"

namespace simu5g {

/**
 * NR UE PHY. Behaviorally identical to LtePhyUe (the receive path was unified
 * into the base); the class is kept as the NR-leg discriminator -- the
 * dynamic_cast<NrPhyUe *> in HandoverController tells the two PHY legs of a
 * dual-stack UE apart.
 */
class NrPhyUe : public LtePhyUe
{
};

} //namespace

#endif /* _NRPHYUE_H_ */

