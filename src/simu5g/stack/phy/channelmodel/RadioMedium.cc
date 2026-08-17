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

    // establish this carrier leg's physics from the first radio to register
    // on it, or check that this radio agrees with what an earlier one
    // established (plan 3(h): frequency alone conflates an NR UE's vestigial
    // LTE leg with the gNB it shares a default component carrier with, and a
    // dual-connectivity master eNB with its secondary gNB)
    CarrierLeg leg{descriptor.carrierFrequency, endpoint->isNr()};
    CarrierPhysics candidate = readCarrierPhysics(endpoint);
    auto cpIt = carrierPhysics_.find(leg);
    if (cpIt == carrierPhysics_.end()) {
        candidate.establishedByPath = endpoint->getFullPath();
        carrierPhysics_[leg] = candidate;
    }
    else {
        checkCarrierPhysics(cpIt->second, candidate, leg, endpoint->getFullPath());
    }

    radios_.push_back(descriptor);
    radioIndex_[key] = &radios_.back();
}

CarrierPhysics RadioMedium::readCarrierPhysics(StochasticChannelModel *endpoint) const
{
    CarrierPhysics cp;
    cp.pathLossType = endpoint->par("pathLossType").stringValue();
    cp.scenario = endpoint->par("scenario").stringValue();
    cp.shadowing = endpoint->par("shadowing");
    cp.correlationDistance = endpoint->par("correlationDistance");
    cp.dynamicLos = endpoint->par("dynamicLos");
    cp.fixedLos = endpoint->par("fixedLos");
    cp.enableExtCellLos = endpoint->par("enableExtCellLos");
    cp.fading = endpoint->par("fading");
    cp.fadingType = endpoint->par("fadingType").stringValue();
    cp.numFadingPaths = endpoint->par("numFadingPaths");
    cp.delayRms = endpoint->par("delayRms");
    cp.thermalNoise = endpoint->par("thermalNoise");
    cp.nodebHeight = endpoint->par("nodebHeight");
    cp.ueHeight = endpoint->par("ueHeight");
    cp.buildingHeight = endpoint->par("buildingHeight");
    cp.streetWidth = endpoint->par("streetWidth");
    cp.useTorus = endpoint->par("useTorus");
    cp.tolerateMaxDistViolation = endpoint->par("tolerateMaxDistViolation");
    cp.harqReduction = endpoint->par("harqReduction");
    cp.targetBler = endpoint->par("targetBler");
    cp.useBuildingPenetrationHighLossModel = endpoint->par("useBuildingPenetrationHighLossModel");
    cp.bgCellInterference = endpoint->par("bgCellInterference");
    cp.extCellInterference = endpoint->par("extCellInterference");
    cp.downlinkInterference = endpoint->par("downlinkInterference");
    cp.uplinkInterference = endpoint->par("uplinkInterference");
    return cp;
}

namespace {

template<typename T>
void checkCarrierField(const char *name, const T& existing, const T& candidate, const CarrierLeg& leg,
        const std::string& existingPath, const std::string& candidatePath)
{
    if (!(existing == candidate))
        throw cRuntimeError("carrier leg %gGHz/%s: parameter '%s' differs between registered radios '%s' and '%s'",
                leg.carrierFrequency.get(), leg.isNr ? "NR" : "LTE", name, existingPath.c_str(), candidatePath.c_str());
}

} // namespace

void RadioMedium::checkCarrierPhysics(const CarrierPhysics& existing, const CarrierPhysics& candidate,
        const CarrierLeg& leg, const std::string& candidatePath) const
{
#define CHECK_CARRIER_FIELD(field) \
    checkCarrierField(#field, existing.field, candidate.field, leg, existing.establishedByPath, candidatePath)

    CHECK_CARRIER_FIELD(pathLossType);
    CHECK_CARRIER_FIELD(scenario);
    CHECK_CARRIER_FIELD(shadowing);
    CHECK_CARRIER_FIELD(correlationDistance);
    CHECK_CARRIER_FIELD(dynamicLos);
    CHECK_CARRIER_FIELD(fixedLos);
    CHECK_CARRIER_FIELD(enableExtCellLos);
    CHECK_CARRIER_FIELD(fading);
    CHECK_CARRIER_FIELD(fadingType);
    CHECK_CARRIER_FIELD(numFadingPaths);
    CHECK_CARRIER_FIELD(delayRms);
    CHECK_CARRIER_FIELD(thermalNoise);
    CHECK_CARRIER_FIELD(nodebHeight);
    CHECK_CARRIER_FIELD(ueHeight);
    CHECK_CARRIER_FIELD(buildingHeight);
    CHECK_CARRIER_FIELD(streetWidth);
    CHECK_CARRIER_FIELD(useTorus);
    CHECK_CARRIER_FIELD(tolerateMaxDistViolation);
    CHECK_CARRIER_FIELD(harqReduction);
    CHECK_CARRIER_FIELD(targetBler);
    CHECK_CARRIER_FIELD(useBuildingPenetrationHighLossModel);
    CHECK_CARRIER_FIELD(bgCellInterference);
    CHECK_CARRIER_FIELD(extCellInterference);
    CHECK_CARRIER_FIELD(downlinkInterference);
    CHECK_CARRIER_FIELD(uplinkInterference);

#undef CHECK_CARRIER_FIELD
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
