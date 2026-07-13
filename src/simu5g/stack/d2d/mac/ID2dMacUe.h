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

#ifndef _LTE_ID2DMACUE_H_
#define _LTE_ID2DMACUE_H_

namespace simu5g {

/*
 * Interface implemented by every D2D-capable UE MAC module (LTE and NR variants).
 *
 * Serves as the D2D capability marker: obtain it with
 * dynamic_cast<ID2dMacUe *>(mac); a null result means the UE is not D2D-capable.
 */
class ID2dMacUe
{
  public:
    virtual ~ID2dMacUe() {}
};

} //namespace

#endif
