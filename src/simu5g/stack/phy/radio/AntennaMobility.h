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

#ifndef _LTE_ANTENNAMOBILITY_H_
#define _LTE_ANTENNAMOBILITY_H_

#include <inet/mobility/single/AttachedMobility.h>

namespace simu5g {

/**
 * See the NED documentation of AntennaMobility.
 */
class AntennaMobility : public inet::AttachedMobility
{
  protected:
    // the position is the attached mobility's, at the offset: none is drawn
    virtual void setInitialPosition() override;

  public:
    // the node's position plus the offset as it is: the offset is the antenna's
    // height above the node, and does not turn with the node's orientation
    virtual const inet::Coord& getCurrentPosition() override;
    virtual const inet::Coord& getCurrentVelocity() override;
};

} // namespace simu5g

#endif
