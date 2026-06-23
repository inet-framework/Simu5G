//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef STACK_PHY_CHANNELMODEL_SIONNA_SIONNAMANAGER_H_
#define STACK_PHY_CHANNELMODEL_SIONNA_SIONNAMANAGER_H_

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <inet/common/ModuleRefByPar.h>
#include <inet/common/geometry/common/Coord.h>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class Binder;

//
// Global owner of the Sionna ray-traced channel table (Plan A). See SionnaManager.ned.
//
// The table is built once (lazily, at the end of initialization or on first lookup)
// and holds, per carrier and per ordered (tx, rx) node pair, a vector of path gains
// in dB. The vector has one entry per logical band (perRb) or a single entry
// (wideband). Path gains span Tx port -> Rx port (they already include antenna
// patterns/beamforming), so callers must not re-apply antenna/cable/fading terms.
//
class SionnaManager : public cSimpleModule
{
  public:
    enum class Granularity { PER_RB, WIDEBAND };
    enum class InterferenceMode { NOISE_LIMITED, ALL_PAIRS };

  protected:
    inet::ModuleRefByPar<Binder> binder_;

    // parameters
    std::string carrierAggregationModulePar_;
    std::string sceneFile_;
    double groundPermittivity_ = 5.0;
    double groundConductivity_ = 0.001;
    double sceneSize_ = 2000.0;
    int numReflections_ = 1;
    std::string polarization_;
    Granularity granularity_ = Granularity::PER_RB;
    InterferenceMode interferenceMode_ = InterferenceMode::NOISE_LIMITED;
    std::string channelTableFile_;
    std::string cacheDir_;
    bool forceRegenerate_ = false;
    std::string pythonExecutable_;
    std::string sionnaScript_;
    std::string backend_;

    // state
    bool tableReady_ = false;

    typedef std::pair<MacNodeId, MacNodeId> LinkKey;          // (tx, rx)
    typedef std::map<LinkKey, std::vector<double>> LinkTable; // -> per-band path gain [dB]
    // carrier frequency [Hz, rounded to long long] -> link table
    std::map<long long, LinkTable> table_;

    // node positions (rounded) -> MacNodeId, to resolve a coordinate to a node
    typedef std::array<long long, 3> PosKey;
    std::map<PosKey, MacNodeId> posToId_;

    static long long carrierKey(GHz carrier);
    static PosKey posKey(const inet::Coord& c);
    bool resolveNode(const inet::Coord& c, MacNodeId& out) const;

    // build/load
    void ensureTable();
    static std::string computeHashHex(const std::string& s);
    void runGenerator(const std::string& requestPath, const std::string& outPath);
    void loadTableFromFile(const std::string& path);
    void warnOnCouplingGuard();

  public:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

    Granularity getGranularity() const { return granularity_; }
    InterferenceMode getInterferenceMode() const { return interferenceMode_; }

    // Returns true if a path gain is available for the ordered (tx, rx) pair on the carrier.
    bool hasPair(MacNodeId tx, MacNodeId rx, GHz carrier);

    // Path gain in dB (Tx port -> Rx port) for the given band. For wideband tables,
    // any band index returns the single wideband value. Throws if the pair is absent.
    double getPathGainDb(MacNodeId tx, MacNodeId rx, GHz carrier, unsigned int band);

    // Per-band path-gain vector [dB] for the link between two coordinates (resolved to
    // nodes by position). Path gain is reciprocal, so both orderings are tried.
    // Returns nullptr if either endpoint or the pair is unknown.
    const std::vector<double> *getPathGainVector(const inet::Coord& a, const inet::Coord& b,
            GHz carrier);
};

} //namespace

#endif
