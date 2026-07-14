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

#ifndef _SIDELINK_SLSTATSCOLLECTOR_H_
#define _SIDELINK_SLSTATSCOLLECTOR_H_

#include <map>
#include <utility>
#include <vector>

#include <inet/common/geometry/common/Coord.h>

#include "simu5g/stack/sidelink/common/SlCommon.h"

namespace simu5g {

/**
 * Network-level collector of the TR 37.885 6.1.5 application-level V2X
 * metrics (WP-G): PRR (packet reception ratio) and PIR (packet
 * inter-reception time), both aggregated per transmitter-receiver distance
 * bin. It is an optional module: sidelink PHYs look it up by name
 * ("slStatsCollector" under the network module) and only report when it
 * exists.
 *
 * PRR per bin = sum over transmitted TBs of receivers-in-bin-that-received /
 * sum of receivers-in-bin-at-TX-time. PIR = time between successive
 * successful receptions from the same transmitter at the same receiver,
 * binned by current pair distance.
 */
class SlStatsCollector : public omnetpp::cSimpleModule
{
  protected:
    double binSize_ = 20;        // [m]
    double maxDistance_ = 500;   // [m]
    int numBins_ = 25;

    std::vector<long> denomPerBin_;      // receivers in bin at TX time (PRR denominator)
    std::vector<long> numerPerBin_;      // successful deliveries per bin (PRR numerator)
    std::vector<double> pirSumPerBin_;   // sum of inter-reception times [s]
    std::vector<long> pirCountPerBin_;

    std::map<std::pair<MacNodeId, MacNodeId>, omnetpp::simtime_t> lastReception_;  // (tx, rx) -> last successful delivery time

    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;

    /// Distance bin index for a distance [m]; -1 if beyond maxDistance_,
    /// otherwise (int)(distance / binSize_) clamped to numBins_-1
    int distanceToBin(double distance) const;

  public:
    /// Find the singleton instance under the network module; nullptr if absent
    /// (no dynamic creation, so the collector is strictly opt-in)
    static SlStatsCollector *findInstance();

    /// Called once per new TB at its initial transmission; counts all *other*
    /// registered SL nodes per distance bin into denomPerBin_
    void recordTransmission(MacNodeId txNodeId, const inet::Coord& txCoord);

    /// Called once per successfully delivered TB at a receiver (duplicates are
    /// already suppressed by the caller)
    void recordDelivery(MacNodeId txNodeId, MacNodeId rxNodeId, const inet::Coord& txCoord, const inet::Coord& rxCoord);
};

} // namespace simu5g

#endif
