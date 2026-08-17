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

#ifndef STACK_PHY_CHANNELMODEL_RADIOMEDIUM_H_
#define STACK_PHY_CHANNELMODEL_RADIOMEDIUM_H_

#include <map>
#include <deque>
#include <utility>

#include <omnetpp.h>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class StochasticChannelModel;

/**
 * One radio endpoint registered with the medium: the endpoint itself, plus
 * the identity the registry indexes it by. Kept minimal -- the accessor
 * surface that reads it is added in a later step.
 */
struct RadioDescriptor
{
    StochasticChannelModel *endpoint = nullptr;
    MacNodeId nodeId = NODEID_NONE;
    GHz carrierFrequency = GHz(0);
};

/**
 * The central, network-level module that models the shared physical radio
 * medium of a cellular network. It is the single owner of the physical facts
 * and channel effects of every radio link, shared by every carrier and every
 * radio endpoint that registers with it.
 */
class RadioMedium : public cSimpleModule
{
  protected:
    // Owns the descriptors; radioIndex_ points into it. Entries keep a stable
    // address across add/remove: a deque's push_back never moves existing
    // elements (a vector's reallocation would dangle every index pointer),
    // and removeRadio() swaps the removed entry with the last one and
    // re-indexes the moved entry instead of shifting the tail.
    std::deque<RadioDescriptor> radios_;
    std::map<std::pair<MacNodeId, GHz>, RadioDescriptor *> radioIndex_;

  public:
    void initialize() override {}

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /** Registers a radio endpoint on its carrier. Duplicate registration is an error. */
    virtual void addRadio(StochasticChannelModel *endpoint);

    /** Unregisters a radio endpoint previously added with addRadio(). */
    virtual void removeRadio(StochasticChannelModel *endpoint);
};

} //namespace

#endif
