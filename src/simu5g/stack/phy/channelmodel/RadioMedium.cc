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

#include "simu5g/stack/phy/channelmodel/RadioMedium.h"

#include <algorithm>

#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"

namespace simu5g {

Define_Module(RadioMedium);

void RadioMedium::handleMessage(cMessage *msg)
{
    throw cRuntimeError("unexpected message '%s': RadioMedium has no gates and schedules no self-messages", msg->getName());
}

void RadioMedium::addRadio(StochasticChannelModel *endpoint)
{
    ASSERT(endpoint != nullptr);

    RadioDescriptor descriptor;
    descriptor.endpoint = endpoint;
    descriptor.nodeId = endpoint->getNodeId();
    descriptor.carrierFrequency = endpoint->getCarrierFrequency();

    auto key = std::make_pair(descriptor.nodeId, descriptor.carrierFrequency);
    if (radioIndex_.find(key) != radioIndex_.end())
        throw cRuntimeError("addRadio: node %d is already registered on carrier %gGHz",
                num(descriptor.nodeId), descriptor.carrierFrequency.get());

    radios_.push_back(descriptor);
    radioIndex_[key] = &radios_.back();
}

void RadioMedium::removeRadio(StochasticChannelModel *endpoint)
{
    ASSERT(endpoint != nullptr);

    auto it = std::find_if(radios_.begin(), radios_.end(),
            [endpoint](const RadioDescriptor& d) { return d.endpoint == endpoint; });
    if (it == radios_.end())
        throw cRuntimeError("removeRadio: endpoint was never registered with this medium");

    radioIndex_.erase(std::make_pair(it->nodeId, it->carrierFrequency));

    // swap-and-pop: keeps every other descriptor's address stable, then
    // re-points the index entry of whichever descriptor took the removed
    // one's place (if the removed one was not already the last)
    size_t idx = it - radios_.begin();
    radios_[idx] = radios_.back();
    radios_.pop_back();
    if (idx < radios_.size())
        radioIndex_[std::make_pair(radios_[idx].nodeId, radios_[idx].carrierFrequency)] = &radios_[idx];
}

const RadioDescriptor& RadioMedium::descriptorFor(MacNodeId nodeId, GHz carrierFrequency) const
{
    auto it = radioIndex_.find(std::make_pair(nodeId, carrierFrequency));
    if (it == radioIndex_.end())
        throw cRuntimeError("no radio registered for node %d on carrier %gGHz", num(nodeId), carrierFrequency.get());
    return *it->second;
}

inet::Coord RadioMedium::coordOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getCoord();
}

double RadioMedium::txPowerOf(MacNodeId nodeId, GHz carrierFrequency, Direction dir) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getTxPwr(dir);
}

TxDirectionType RadioMedium::txDirectionOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getTxDirection();
}

double RadioMedium::txAngleOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getTxAngle();
}

double RadioMedium::antennaGainOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getAntennaGain();
}

double RadioMedium::noiseFigureOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getNoiseFigure();
}

double RadioMedium::insideDistanceOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getInsideDistance();
}

} //namespace
