//
//                  Simu5G
//
// Copyright (C) 2026 Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/phy/radio/AntennaMobility.h"

namespace simu5g {

Define_Module(AntennaMobility);

void AntennaMobility::setInitialPosition()
{
    lastPosition = inet::Coord::ZERO;
}

const inet::Coord& AntennaMobility::getCurrentPosition()
{
    lastPosition = mobility->getCurrentPosition() + positionOffset;
    return lastPosition;
}

const inet::Coord& AntennaMobility::getCurrentVelocity()
{
    lastVelocity = mobility->getCurrentVelocity();
    return lastVelocity;
}

} // namespace simu5g
