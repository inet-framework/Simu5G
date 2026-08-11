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

#ifndef _NRMACGNB_H_
#define _NRMACGNB_H_

#include "simu5g/stack/mac/LteMacEnb.h"

namespace simu5g {

class NrMacGnb : public LteMacEnb
{
  public:

    NrMacGnb();

  protected:
    // The pre-separation NR gNB followed the historical LteMacEnbD2D "fork" of
    // sendGrants()/macPduUnmake() rather than plain LteMacEnb. Those methods now
    // live once in LteMacEnb; of the three seams that preserved the fork, the
    // grant direction and the BSR buffer key have become the base's own behavior,
    // leaving only the grant header length below -- now the base's value too.
    inet::b grantChunkLength() const override { return inet::B(1); }
};

} //namespace

#endif

