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
class CellularInterferenceModel;
class D2dChannelModel;
class TrafficGeneratorBase;

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
 * A background transmitter's phantom key (plan 3(j)): the tuple that *is*
 * network-unique, since its own MacNodeId is not -- every
 * BackgroundTrafficManager numbers its population from BGUE_MIN_ID, so the
 * first background UE of every manager in the network is MacNodeId(4097).
 * cellId is the owning eNB/gNB's MacCellId (mac_->getMacCellId() on the
 * registering manager), matching what LteSchedulerEnb stores alongside that
 * same background UE's allocation.
 */
struct BgUeKey
{
    MacCellId cellId = NODEID_NONE;
    GHz carrierFrequency = GHz(0);
    MacNodeId bgUeId = NODEID_NONE;

    bool operator<(const BgUeKey& o) const
    {
        if (cellId != o.cellId)
            return cellId < o.cellId;
        if (carrierFrequency != o.carrierFrequency)
            return carrierFrequency < o.carrierFrequency;
        return bgUeId < o.bgUeId;
    }
};

/**
 * One radio endpoint registered with the medium: the endpoint itself, plus
 * the identity the registry indexes it by. Kept minimal -- the accessor
 * surface that reads it is added in a later step.
 */
struct RadioDescriptor
{
    StochasticChannelModel *endpoint = nullptr;   // null iff this is a phantom (plan 3(j))
    MacNodeId nodeId = NODEID_NONE;
    GHz carrierFrequency = GHz(0);

    // The D2D marker (plan S12a): endpoint downcast once at registration,
    // non-null iff this radio is D2D-capable. The registration-time
    // dynamic_cast this is built from is the only one -- every D2D-aware
    // branch reads this cached pointer afterward instead of casting itself,
    // and it doubles as the handle onto D2D-only facts (isD2DInterferenceEnabled(),
    // the rcvdSinrD2D signal, getSINR_D2D()) that plain StochasticChannelModel
    // does not carry.
    D2dChannelModel *d2dEndpoint = nullptr;

    // The phantom half (plan S12b/3(j)): non-null iff endpoint == nullptr --
    // ASSERTed at registration that exactly one of the two is non-null. A
    // phantom is never reachable through radioIndex_/descriptorFor(MacNodeId,
    // GHz) -- it lives in bgRadioIndex_ instead, keyed by BgUeKey, the tuple
    // that is actually unique. nodeId/carrierFrequency above still carry the
    // phantom's own bgUeId and carrier (for reindex() and BgUeKey's other two
    // fields); bgCellId is the third component, the owning cell, that neither
    // field carries.
    TrafficGeneratorBase *bgGenerator = nullptr;
    MacCellId bgCellId = NODEID_NONE;
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

    // Phantom radios (plan S12b/3(j)), keyed by the tuple that is unique --
    // real radios never enter this index, and a phantom never enters
    // radioIndex_. Same radios_ deque, same lifetime story, same
    // swap-and-pop removal; reindex() is what keeps a moved descriptor
    // pointed at from the right index.
    std::map<BgUeKey, RadioDescriptor *> bgRadioIndex_;

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

    // This medium's own interference submodule (S2's slot, S11's payload):
    // resolved once at initialize(), read live thereafter. Not owned -- it
    // is a child module, torn down by the module hierarchy like any other.
    CellularInterferenceModel *interference_ = nullptr;

    /** Looks up the registered radio for (nodeId, carrierFrequency); throws if none is registered. */
    const RadioDescriptor& descriptorFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /** Looks up the registered phantom for key; throws if none is registered (plan S12b). */
    const RadioDescriptor& descriptorFor(const BgUeKey& key) const;

    /**
     * Re-points radioIndex_ or bgRadioIndex_ (dispatching on endpoint != nullptr)
     * for descriptor, whose address just changed under swap-and-pop removal.
     * Factored so both removal paths use it: using removeRadio's own re-index
     * line unconditionally would write a phantom into the real-radio index
     * (plan S12b item 5 / risk 18).
     */
    void reindex(RadioDescriptor& descriptor);

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

    /** Resolves interference_, this medium's own interference submodule (S2/S11). */
    void initialize() override;

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /** Registers a radio endpoint on its carrier. Duplicate registration is an error. */
    virtual void addRadio(StochasticChannelModel *endpoint);

    /** Unregisters a radio endpoint previously added with addRadio(). */
    virtual void removeRadio(StochasticChannelModel *endpoint);

    /**
     * Registers a background transmitter's phantom radio under key, the
     * tuple that is unique (plan S12b/3(j)): unlike addRadio(), this
     * establishes no CarrierPhysics record and creates no PathLossModel or
     * per-radio stochastic state -- a phantom declares none of the 25
     * per-carrier-leg physics parameters and owns no stochastic state.
     * Duplicate registration is an error, exactly as addRadio()'s is.
     */
    virtual void addBackgroundRadio(const BgUeKey& key, TrafficGeneratorBase *generator);

    /** Unregisters a background transmitter's phantom radio previously added with addBackgroundRadio(). */
    virtual void removeBackgroundRadio(const BgUeKey& key);

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

    /**
     * Physical facts of a registered background transmitter (plan S12b/3(j)):
     * the accessor family widens to the phantom key rather than forking --
     * two overloads on the existing names, not new ones. No Direction
     * overload of txPowerOf: TrafficGeneratorBase::getTxPwr() takes none, a
     * background UE has one tx power, and today's branch this replaces
     * passes no direction either.
     */
    virtual inet::Coord coordOf(const BgUeKey& key) const;
    virtual double txPowerOf(const BgUeKey& key) const;

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
     * another RNG canary. computeInterferencePlusNoise calls interference_
     * (this medium's own submodule), never radio, for the four cellular
     * walks (S11) and, since S12, for the D2D walk too.
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

    /**
     * D2D-aware branches folded in from D2dChannelModel (plan step S12):
     * D2dChannelModel becomes an endpoint marker (RadioDescriptor::d2dEndpoint),
     * and getReceptionSinr/emitRcvdSinr/computeInterferencePlusNoise stop being
     * virtual dispatch points -- there is nothing left overriding them, so
     * isReceptionSuccessful/getSINR call them as plain sibling methods here
     * instead of through radio. d2dLink is the link-builder counterpart of
     * linkFor/cellularLink (S9b/S10 shape); computeD2DInterference is
     * interference-walk-shaped and lives beside its four cellular siblings on
     * interference_ (S11 shape) instead. Neither draws; the only random draw
     * a D2D reception depends on is getAttenuation's, reached through radio's
     * own carrier leg exactly like a cellular link's.
     */
    virtual RadioLink d2dLink(StochasticChannelModel *radio, MacNodeId srcId, inet::Coord srcCoord,
            MacNodeId destId, inet::Coord destCoord, bool useUeSideMaps);
    virtual std::vector<double> getReceptionSinr(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector);
    virtual void emitRcvdSinr(StochasticChannelModel *radio, Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr);
};

} //namespace

#endif
