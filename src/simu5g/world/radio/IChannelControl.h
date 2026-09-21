//
//                  Simu5G
//
// Copyright (C) Rudolf Hornig
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef ICHANNELCONTROL_H
#define ICHANNELCONTROL_H

#include <vector>
#include <list>
#include <set>

#include <inet/common/INETDefs.h>
#include <inet/common/geometry/common/Coord.h>
#include "simu5g/common/LteDefs.h"

namespace simu5g {

using namespace omnetpp;

class AirFrame;

/**
 * Interface to implement for a module that controls radio frequency channel access.
 */
class IChannelControl
{
  protected:
    struct RadioEntry;

  public:
    typedef RadioEntry *RadioRef; // handle for ChannelControl's clients

  public:

    /** Registers the given radio. If radioInGate==nullptr, the "radioIn" gate is assumed */
    virtual RadioRef registerRadio(cModule *radioModule, cGate *radioInGate = nullptr) = 0;

    /** Unregisters the given radio */
    virtual void unregisterRadio(RadioRef r) = 0;

    /** Returns the host module that contains the given radio */
    virtual cModule *getRadioModule(RadioRef r) const = 0;

    /** Returns the input gate of the host for receiving AirFrames */
    virtual cGate *getRadioGate(RadioRef r) const = 0;

    /** To be called when the host moved; updates proximity info */
    virtual void setRadioPosition(RadioRef r, const inet::Coord& pos) = 0;

    /** Called from ChannelAccess, to transmit a frame to the radios in range */
    virtual void sendToChannel(RadioRef srcRadio, AirFrame *airFrame) = 0;

    /** Returns the maximal interference distance */
    virtual double getInterferenceRange(RadioRef r) = 0;

    /** Returns propagation speed of the signal in meters/sec */
    virtual double getPropagationSpeed() = 0;
};

} //namespace

#endif

