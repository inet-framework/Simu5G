//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/common/SlStatsCollector.h"

#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/phy/LtePhyBase.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(SlStatsCollector);

void SlStatsCollector::initialize()
{
    binSize_ = par("binSize").doubleValueInUnit("m");
    maxDistance_ = par("maxDistance").doubleValueInUnit("m");
    numBins_ = (int)ceil(maxDistance_ / binSize_);

    denomPerBin_.resize(numBins_, 0);
    numerPerBin_.resize(numBins_, 0);
    pirSumPerBin_.resize(numBins_, 0);
    pirCountPerBin_.resize(numBins_, 0);
}

void SlStatsCollector::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

SlStatsCollector *SlStatsCollector::findInstance()
{
    cModule *network = cSimulation::getActiveSimulation()->getSystemModule();
    cModule *m = network->getSubmodule("slStatsCollector");
    return dynamic_cast<SlStatsCollector *>(m);  // nullptr if absent; no dynamic creation
}

int SlStatsCollector::distanceToBin(double distance) const
{
    if (distance > maxDistance_)
        return -1;
    int bin = (int)(distance / binSize_);
    if (bin >= numBins_)
        bin = numBins_ - 1;
    return bin;
}

void SlStatsCollector::recordTransmission(MacNodeId txNodeId, const inet::Coord& txCoord)
{
    Enter_Method_Silent("recordTransmission()");

    // count every *other* registered SL node into the distance bin it falls in
    for (const auto& entry : SlBinder::getInstance()->getSlPhys()) {
        if (entry.first == txNodeId)
            continue;
        LtePhyBase *phy = check_and_cast<LtePhyBase *>(entry.second);
        const inet::Coord& coord = phy->getCoord();
        double distance = txCoord.distance(coord);
        int bin = distanceToBin(distance);
        if (bin >= 0)
            denomPerBin_[bin]++;
    }
}

void SlStatsCollector::recordDelivery(MacNodeId txNodeId, MacNodeId rxNodeId, const inet::Coord& txCoord, const inet::Coord& rxCoord)
{
    Enter_Method_Silent("recordDelivery()");

    int bin = distanceToBin(txCoord.distance(rxCoord));
    if (bin >= 0)
        numerPerBin_[bin]++;

    // PIR: inter-reception time between successive successful deliveries from
    // the same transmitter at the same receiver
    auto key = std::make_pair(txNodeId, rxNodeId);
    auto it = lastReception_.find(key);
    if (it != lastReception_.end()) {
        simtime_t pir = simTime() - it->second;
        if (bin >= 0) {
            pirSumPerBin_[bin] += pir.dbl();
            pirCountPerBin_[bin]++;
        }
    }
    lastReception_[key] = simTime();
}

void SlStatsCollector::finish()
{
    char name[64];

    long totalNumer = 0;
    long totalDenom = 0;

    for (int i = 0; i < numBins_; i++) {
        int low = (int)(i * binSize_);
        int high = (int)((i + 1) * binSize_);

        if (denomPerBin_[i] > 0) {
            snprintf(name, sizeof(name), "prr d=[%d,%d)m", low, high);
            recordScalar(name, (double)numerPerBin_[i] / denomPerBin_[i]);
        }

        if (pirCountPerBin_[i] > 0) {
            snprintf(name, sizeof(name), "pir d=[%d,%d)m", low, high);
            recordScalar(name, pirSumPerBin_[i] / pirCountPerBin_[i]);
        }

        totalNumer += numerPerBin_[i];
        totalDenom += denomPerBin_[i];
    }

    if (totalDenom > 0)
        recordScalar("prr total", (double)totalNumer / totalDenom);
}

} // namespace simu5g
