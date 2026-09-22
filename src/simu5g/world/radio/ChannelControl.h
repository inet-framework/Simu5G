//
//                  Simu5G
//
// Copyright (C) 2006 Levente Meszaros, 2005 Andras Varga
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef CHANNELCONTROL_H
#define CHANNELCONTROL_H

#include <vector>
#include <list>
#include <set>

#include <inet/common/INETDefs.h>
#include <inet/common/geometry/common/Coord.h>

namespace simu5g {

using namespace omnetpp;

// Forward declarations
class AirFrame;

/**
 * Monitors which radios are "in range".
 *
 * @ingroup channelControl
 * @see ChannelAccess
 */
class ChannelControl : public cSimpleModule
{
  protected:
    struct RadioEntry;

  public:
    typedef RadioEntry *RadioRef; // handle for ChannelControl's clients

  protected:
    /**
     * Keeps track of radios/NICs and their positions;
     * also caches neighbor info (which other Radios are within
     * interference distance).
     */
    struct RadioEntry {
        opp_component_ptr<cModule> radioModule;  // the module that registered this radio interface
        cGate *radioInGate = nullptr;  // gate on host module used to receive airframes
        inet::Coord pos; // cached radio position

        struct Compare {
            bool operator()(const RadioRef& lhs, const RadioRef& rhs) const {
                ASSERT(lhs != nullptr);
                ASSERT(rhs != nullptr);
                return lhs->radioModule->getId() < rhs->radioModule->getId();
            }
        };
        // we cache neighbors set in a std::vector, because std::set iteration is slow;
        // std::vector is created and updated on demand
        std::set<RadioRef, Compare> neighbors; // cached neighbor list
        std::vector<RadioRef> neighborList;
        bool isNeighborListValid;
    };

    typedef std::list<RadioEntry> RadioList;
    typedef std::vector<RadioRef> RadioRefVector;

    RadioList radios;

    friend std::ostream& operator<<(std::ostream&, const RadioEntry&);

    /** the maximum interference distance in the network.*/
    double maxInterferenceDistance;

  protected:
    virtual void updateConnections(RadioRef h);

    /** Calculate interference distance*/
    virtual double calcInterfDist();

    /** Reads init parameters and calculates a maximum interference distance*/
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    /** Get the list of modules in range of the given host */
    virtual const RadioRefVector& getNeighbors(RadioRef h);

    /** Returns the "handle" of a previously registered radio. The pointer to the registering (radio) module must be provided */
    virtual RadioRef lookupRadio(cModule *radioModule);

  public:
    /** Registers the given radio. If radioInGate==NULL, the "radioIn" gate is assumed */
    virtual RadioRef registerRadio(cModule *radioModule, cGate *radioInGate = nullptr);

    /** Unregisters the given radio */
    virtual void unregisterRadio(RadioRef r);

    /** Returns the host module that contains the given radio */
    virtual cModule *getRadioModule(RadioRef r) const { return r->radioModule; }

    /** Returns the input gate of the host for receiving AirFrames */
    virtual cGate *getRadioGate(RadioRef r) const { return r->radioInGate; }

    /** To be called when the host moved; updates proximity info */
    virtual void setRadioPosition(RadioRef r, const inet::Coord& pos);

    /** Called from ChannelAccess, to transmit a frame to the radios in range */
    virtual void sendToChannel(RadioRef srcRadio, AirFrame *airFrame);

    /** Returns the maximum interference distance*/
    virtual double getInterferenceRange(RadioRef r) { return maxInterferenceDistance; }

    /** Returns propagation speed of the signal in meters/sec */
    virtual double getPropagationSpeed() { return SPEED_OF_LIGHT; }
};

} //namespace

#endif

