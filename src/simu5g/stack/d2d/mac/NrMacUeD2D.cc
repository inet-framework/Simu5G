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

#include "simu5g/stack/d2d/mac/NrMacUeD2D.h"

namespace simu5g {

Define_Module(NrMacUeD2D);

// the signal name is interned at runtime here -- see the header for why there
// is deliberately NO static registerSignal() in this translation unit
NrMacUeD2D::NrMacUeD2D() : D2dUeMacBase<NrMacUe>(cComponent::registerSignal("rcvdD2DModeSwitchNotification"))
{
}

} //namespace
