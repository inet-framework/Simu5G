//
//                  Simu5G
//
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_PHY_RADIO_CELLULARANTENNA_H_
#define STACK_PHY_RADIO_CELLULARANTENNA_H_

#include <omnetpp.h>
#include <inet/physicallayer/wireless/common/antenna/IsotropicAntenna.h>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

/**
 * See the NED documentation of CellularAntenna.
 */
class CellularAntenna : public inet::physicallayer::IsotropicAntenna
{
  protected:
    double gain_ = NAN;
    TxDirectionType txDirection_ = OMNI;
    double txAngle_ = NAN;

  protected:
    virtual void initialize(int stage) override;

  public:
    /** Antenna gain in dBi. */
    double getGainDb() const { return gain_; }
    /** Whether the antenna radiates omnidirectionally or sectorially. */
    TxDirectionType getTxDirection() const { return txDirection_; }
    /** The boresight of a sectorial antenna, in degrees; 0 for an omnidirectional one. */
    double getTxAngle() const { return txAngle_; }
};

} // namespace simu5g

#endif
