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

#include "simu5g/stack/d2d/mac/amc/NrAmcD2D.h"

namespace simu5g {

Define_Module(NrAmcD2D);

NrMcsTable *NrAmcD2D::getNrMcsTableForDirection(Direction dir)
{
    if (dir == D2D || dir == D2D_MULTI)
        return &ulNrMcsTable_;
    return NrAmc::getNrMcsTableForDirection(dir);
}

} //namespace
