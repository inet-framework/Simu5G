//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <inet/common/stlutils.h>
#include <inet/networklayer/common/NetworkInterface.h>

#include "simu5g/stack/d2d/binder/D2dBinder.h"
#include "simu5g/stack/d2d/mac/ID2dMacUe.h"
#include "simu5g/stack/mac/LteMacBase.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;

Define_Module(D2dBinder);

D2dBinder *D2dBinder::getInstance(cModule *contextModule)
{
    cModule *network = contextModule->getSimulation()->getSystemModule();
    cModule *mod = network->getSubmodule("d2dBinder");
    if (mod == nullptr) {
        // Find-or-create: the network NED declares no 'd2dBinder' submodule, so
        // create it dynamically now. The module is event-less, so initializing
        // it immediately (even during another module's init) is fingerprint-inert.
        cModuleType *type = cModuleType::get("simu5g.stack.d2d.binder.D2dBinder");
        mod = type->createScheduleInit("d2dBinder", network);
    }
    return check_and_cast<D2dBinder *>(mod);
}

void D2dBinder::initialize()
{
    binder_.reference(this, "binderModule", true);
    WATCH_SET(multicastTransmitterSet_);
}

LteD2DMode D2dBinder::computeD2DCapability(MacNodeId src, MacNodeId dst)
{
    LteMacBase *dstMac = binder_->getMacFromMacNodeId(dst);
    if (dynamic_cast<ID2dMacUe *>(dstMac) != nullptr) {
        // set the initial mode
        if (binder_->getServingNode(src) == binder_->getServingNode(dst)) {
            // if served by the same cell, then the mode is selected according to the corresponding parameter
            LteMacBase *srcMac = binder_->getMacFromMacNodeId(src);
            inet::NetworkInterface *srcNic = getContainingNicModule(srcMac);
            bool d2dInitialMode = srcNic->hasPar("d2dInitialMode") ? srcNic->par("d2dInitialMode").boolValue() : false;
            return d2dInitialMode ? DM : IM;
        }
        else {
            // if served by different cells, then the mode can be IM only
            return IM;
        }
    }
    else {
        // this is not a D2D-capable flow
        return NONE;
    }
}

bool D2dBinder::checkD2DCapability(MacNodeId src, MacNodeId dst)
{
    ASSERT(getNodeTypeById(src) == UE && binder_->nodeExists(src));
    ASSERT(getNodeTypeById(dst) == UE && binder_->nodeExists(dst));

    // if the entry is missing, check if the receiver is D2D capable and update the map
    if (!containsKey(d2dPeeringMap_, src) || !containsKey(d2dPeeringMap_[src], dst)) {
        LteD2DMode mode = computeD2DCapability(src, dst);
        if (mode == NONE) {
            EV << "D2dBinder::checkD2DCapability - UE " << src << " may not transmit to UE " << dst << " using D2D (UE " << dst << " is not D2D capable)" << endl;
        }
        else {
            EV << "D2dBinder::checkD2DCapability - UE " << src << " may transmit to UE " << dst << " using D2D (current mode " << (mode == DM ? "DM)" : "IM)") << endl;
        }
        d2dPeeringMap_[src][dst] = mode;
        return mode != NONE;
    }

    // if an entry is present, and it is not NONE, this is a D2D-capable flow
    return d2dPeeringMap_[src][dst] != NONE;
}

bool D2dBinder::getD2DCapability(MacNodeId src, MacNodeId dst)
{
    ASSERT(getNodeTypeById(src) == UE && binder_->nodeExists(src));
    ASSERT(getNodeTypeById(dst) == UE && binder_->nodeExists(dst));

    // return true if the entry exists and it is not NONE, no matter if it is DM or IM
    return containsKey(d2dPeeringMap_, src) && containsKey(d2dPeeringMap_[src], dst) && d2dPeeringMap_[src][dst] != NONE;
}

LteD2DMode D2dBinder::getD2DMode(MacNodeId src, MacNodeId dst)
{
    if (!getD2DCapability(src, dst))
        throw cRuntimeError("D2dBinder::getD2DMode - Node Id not valid. Src %hu Dst %hu", num(src), num(dst));

    return d2dPeeringMap_[src][dst];
}

void D2dBinder::setD2DMode(MacNodeId src, MacNodeId dst, LteD2DMode mode)
{
    d2dPeeringMap_[src][dst] = mode;
}

void D2dBinder::addD2DMulticastTransmitter(MacNodeId nodeId)
{
    multicastTransmitterSet_.insert(nodeId);
}

std::set<MacNodeId>& D2dBinder::getD2DMulticastTransmitters()
{
    return multicastTransmitterSet_;
}

} //namespace
