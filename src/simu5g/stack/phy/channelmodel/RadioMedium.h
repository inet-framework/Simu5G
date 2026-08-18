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

#include <deque>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <omnetpp.h>
#include <inet/common/geometry/common/Coord.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/stack/phy/channelmodel/ChannelModelBase.h"
#include "simu5g/stack/phy/channelmodel/PathLossModel.h"

namespace simu5g {

using namespace omnetpp;

class StochasticChannelModel;

/** A timestamped position: when it was recorded, and where. */
typedef std::pair<inet::simtime_t, inet::Coord> Position;

/** One fading path's angle of arrival and delay spread, drawn once per link per band. */
struct JakesFadingData
{
    std::vector<double> angleOfArrival;
    std::vector<simtime_t> delaySpread;
};

typedef std::map<LinkKey, std::vector<JakesFadingData>> JakesFadingMap;
typedef std::map<LinkKey, std::pair<inet::simtime_t, double>> ShadowFadingMap;

/**
 * The stochastic state a StochasticChannelModel endpoint used to keep in its
 * own member variables (losMap_, lastComputedSF_, jakesFadingMap_,
 * jakesFadingMapBgUe_, positionHistory_, lastCorrelationPoint_), relocated
 * here verbatim: same containers, same key types, no merging, no re-keying
 * (the per-link re-key is a later step). One instance per registered radio.
 */
struct PerRadioStochasticState
{
    std::map<LinkKey, bool> losMap;
    ShadowFadingMap lastComputedSF;
    JakesFadingMap jakesFadingMap;
    JakesFadingMap jakesFadingMapBgUe;
    std::map<MacNodeId, std::queue<Position>> positionHistory;
    std::map<LinkKey, Position> lastCorrelationPoint;
};

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
 * The unit CarrierPhysics is grouped by: a carrier frequency alone conflates
 * an NR UE's always-instantiated but unused LTE leg with the gNB it shares a
 * default component-carrier frequency with, and conflates a dual-connectivity
 * master eNB with its secondary gNB (plan section 3(h)). Frequency stays the
 * primary component; isNr is the added discriminator.
 */
struct CarrierLeg
{
    GHz carrierFrequency = GHz(0);
    bool isNr = false;

    bool operator<(const CarrierLeg& o) const
    {
        return carrierFrequency != o.carrierFrequency ? carrierFrequency < o.carrierFrequency : isNr < o.isNr;
    }
};

/**
 * The per-carrier-leg physics parameters (plan section 3(e)/3(h)) that every
 * radio registered on a given leg must agree on. Filled from the first radio
 * to register on that leg; every later one is checked against it,
 * field by field, in addRadio(). Per-radio parameters (antenna gains, cable
 * loss, noise figures, insideBuilding and the module-path parameters) are
 * deliberately not part of this record.
 */
struct CarrierPhysics
{
    std::string pathLossType;
    std::string scenario;
    bool shadowing = false;
    double correlationDistance = 0;
    bool dynamicLos = false;
    bool fixedLos = false;
    bool enableExtCellLos = false;
    bool fading = false;
    std::string fadingType;
    int numFadingPaths = 0;
    double delayRms = 0;
    double thermalNoise = 0;
    double nodebHeight = 0;
    double ueHeight = 0;
    double buildingHeight = 0;
    double streetWidth = 0;
    bool useTorus = false;
    bool tolerateMaxDistViolation = false;
    double harqReduction = 0;
    double targetBler = 0;
    bool useBuildingPenetrationHighLossModel = false;
    bool bgCellInterference = false;
    bool extCellInterference = false;
    bool downlinkInterference = false;
    bool uplinkInterference = false;

    // full path of the radio whose parameters filled this record, named in
    // a mismatch error alongside the radio that disagrees with it
    std::string establishedByPath;
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

    // One CarrierPhysics record per carrier leg, established by the first
    // radio to register on it (see addRadio()). Nothing reads this yet.
    std::map<CarrierLeg, CarrierPhysics> carrierPhysics_;

    // Per-radio stochastic state (S8), keyed by the endpoint pointer itself
    // rather than by position in radios_/radioIndex_: RadioDescriptor entries
    // move on swap-and-pop removal, but the endpoint's own identity is stable
    // for exactly its registered lifetime, which is what a std::map's node
    // storage needs to hand out references (via stateOf()) that survive
    // other radios' registration and removal. Erased in removeRadio().
    std::map<StochasticChannelModel *, PerRadioStochasticState> radioState_;

    // One PathLossModel strategy per carrier leg (S9b), created eagerly in
    // addRadio() when a leg's CarrierPhysics record is first established.
    // PathLossModel::owner_ is this medium, which is what relocates every
    // propagation-formula random draw onto the medium's own rng-0 stream.
    // Owned; leg records are never removed (like carrierPhysics_), so a
    // strategy lives for the run.
    std::map<CarrierLeg, PathLossModel *> pathLoss_;

    /** Looks up the registered radio for (nodeId, carrierFrequency); throws if none is registered. */
    const RadioDescriptor& descriptorFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /** Reads the per-carrier-leg physics parameters (plan 3(e)) declared on the endpoint's own NED type. */
    CarrierPhysics readCarrierPhysics(StochasticChannelModel *endpoint) const;

    /** Checks candidate against the leg's established record; throws naming the first mismatched parameter. */
    void checkCarrierPhysics(const CarrierPhysics& existing, const CarrierPhysics& candidate,
            const CarrierLeg& leg, const std::string& candidatePath) const;

    /** The per-leg path-loss strategy established in addRadio(); throws if no radio has registered on the leg. */
    PathLossModel& pathLossFor(const CarrierLeg& leg) const;

    /** The per-leg CarrierPhysics record established in addRadio(); throws if no radio has registered on the leg. */
    const CarrierPhysics& carrierPhysicsFor(const CarrierLeg& leg) const;

    /** Builds the propagation-formula strategy matching cp.pathLossType and initializes it from the leg's own established CarrierPhysics record and carrier frequency (plan 3(i).4). */
    PathLossModel *createPathLossModel(const CarrierPhysics& cp, const CarrierLeg& leg);

  public:
    ~RadioMedium() override;

    void initialize() override {}

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /** Registers a radio endpoint on its carrier. Duplicate registration is an error. */
    virtual void addRadio(StochasticChannelModel *endpoint);

    /** Unregisters a radio endpoint previously added with addRadio(). */
    virtual void removeRadio(StochasticChannelModel *endpoint);

    /**
     * The per-radio stochastic state for a registered endpoint. Created in
     * addRadio(), so an endpoint can cache the reference once at
     * registration (its address is stable across other radios'
     * registration and removal, since it lives in a std::map keyed by
     * endpoint identity); erased in removeRadio().
     */
    virtual PerRadioStochasticState& stateOf(StochasticChannelModel *endpoint);

    // Physical facts of a registered radio, read live through its own
    // endpoint pointer -- the registry resolves identity, it does not cache
    // values.
    virtual inet::Coord coordOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double txPowerOf(MacNodeId nodeId, GHz carrierFrequency, Direction dir = UNKNOWN_DIRECTION) const;
    virtual TxDirectionType txDirectionOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double txAngleOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double antennaGainOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double noiseFigureOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double insideDistanceOf(MacNodeId nodeId, GHz carrierFrequency) const;

    /** The calling radio's outdoor-to-indoor geometry (plan 3(i)), read live off its registered endpoint. */
    virtual O2iState o2iStateOf(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The per-leg path-loss strategy for a registered radio, resolved through
     * the registry like the rest of this accessor family. Lets a still-resident
     * endpoint method (computeAngularAttenuation, whose formula never drew and
     * so was never on S9b's relocation list) keep delegating to the strategy
     * now that pathLoss_ no longer lives on the endpoint.
     */
    virtual PathLossModel& pathLossFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The physical-layer computation relocated from StochasticChannelModel
     * (plan step S9b): the endpoint that used to compute these now holds
     * only a one-line forwarder to here, so every random draw that used to
     * consume the endpoint's own rng-0 stream now consumes this medium's,
     * in the same order and the same count -- the byte-identical relocation
     * the step depends on. radio identifies the calling endpoint, whose own
     * per-radio state (stateOf) and O2I geometry (o2iStateOf) these read;
     * the shared PathLossModel strategy is looked up by radio's carrier leg.
     */
    virtual double getAttenuation(StochasticChannelModel *radio, const RadioLink& link);
    virtual double computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los);
    virtual void computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key);
    virtual double computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key,
            MacNodeId ownerId, double speed, bool cqiDl);
    virtual double jakesFading(StochasticChannelModel *radio, const LinkKey& key, MacNodeId ownerId, double speed,
            unsigned int band, bool cqiDl, bool isBgUe = false);
    virtual double rayleighFading(StochasticChannelModel *radio, MacNodeId id, unsigned int band);
    virtual double computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);
    virtual double computeCorrelationDistance(StochasticChannelModel *radio, const LinkKey& key, const inet::Coord coord);

    /**
     * The SINR/RSRP/reception-decision surface relocated from
     * StochasticChannelModel (plan step S10): same one-line-forwarder shape
     * as the S9b computation above. isReceptionSuccessful's BLER draw
     * (uniform(0.0, 1.0)) moves with it, onto this medium's rng-0 stream --
     * another RNG canary. Where the moved body calls a still-resident
     * StochasticChannelModel method that D2dChannelModel overrides
     * (computeInterferencePlusNoise, getReceptionSinr, emitRcvdSinr), it
     * calls back through radio rather than on itself, so that override
     * still fires for a D2D-capable radio; the four interference walks
     * (S11) are reached the same way, since they too stay resident for now.
     */
    virtual std::vector<double> getSINR(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo);
    virtual std::vector<double> getSINR(StochasticChannelModel *radio, const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector);
    virtual std::vector<double> getSIR(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo);
    virtual std::vector<double> getRSRP(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo);
    virtual std::vector<double> getRSRP(StochasticChannelModel *radio, const RadioLink& link, double txPower);
    virtual std::vector<double> getSINR_bgUe(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo);
    virtual double getReceivedPower_bgUe(StochasticChannelModel *radio, double txPower, inet::Coord txPos, inet::Coord rxPos,
            Direction dir, bool losStatus, MacNodeId bsId);
    virtual void computeInterferencePlusNoise(StochasticChannelModel *radio, const RadioLink& link, UserControlInfo *lteInfo,
            RbMap& rbmap, double totN, std::vector<double>& den);
    virtual bool isReceptionSuccessful(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector);
};

} //namespace

#endif
