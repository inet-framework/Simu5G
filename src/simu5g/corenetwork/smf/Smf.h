//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SMF_H_
#define _SMF_H_

#include <omnetpp.h>

namespace simu5g {

using namespace omnetpp;

/**
 * The Session Management Function of the core network. It has one instance in
 * the whole network. See the NED file for details.
 */
class Smf : public cSimpleModule
{
  protected:
    void handleMessage(cMessage *msg) override { throw cRuntimeError("This module does not process messages"); }
};

} //namespace

#endif
