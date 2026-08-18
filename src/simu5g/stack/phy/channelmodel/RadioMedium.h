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

/**
 * The unit CarrierPhysics -- and, since S13, the medium's per-link stochastic
 * state -- is grouped by: a carrier frequency alone conflates an NR UE's
 * always-instantiated but unused LTE leg with the gNB it shares a default
 * component-carrier frequency with, and conflates a dual-connectivity master
 * eNB with its secondary gNB (plan section 3(h)). Frequency stays the
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
 * Key for the medium's per-link stochastic state (plan S13/§3(b)): the
 * carrier leg plus the unordered pair of nodes the link connects (LinkKey
 * already normalizes that pair). One entry per physical link, shared by
 * both ends -- not one per registering endpoint, which is the per-end
 * asymmetry this step abolishes: the same link no longer draws its own LOS,
 * shadowing and Jakes realization once from each side.
 */
typedef std::pair<CarrierLeg, LinkKey> LinkStateKey;

typedef std::map<LinkStateKey, std::vector<JakesFadingData>> JakesFadingMap;
typedef std::map<LinkStateKey, std::pair<inet::simtime_t, double>> ShadowFadingMap;

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

    // Per-link stochastic state (plan S13/§3(b)), keyed by LinkStateKey --
    // one shared entry per physical link, not one per registering endpoint
    // the way the S8-S12 radioState_ this replaces was. losMap_ and
    // lastComputedSF_/jakesFadingMap_/jakesFadingMapBgUe_ together replace
    // that per-radio scheme's six containers.
    std::map<LinkStateKey, bool> losMap_;
    ShadowFadingMap lastComputedSF_;
    JakesFadingMap jakesFadingMap_;
    JakesFadingMap jakesFadingMapBgUe_;

    // Per-(node, CarrierLeg) mobility state (plan S13/§3(b)): a node's motion
    // is a property of the node itself, not of a link between two nodes, so
    // these stay keyed one level coarser than the four containers above --
    // shared across every link that tracks the same node on the same leg,
    // rather than once per (observing endpoint, node) as before S13.
    std::map<std::pair<MacNodeId, CarrierLeg>, std::queue<Position>> positionHistory_;
    std::map<std::pair<MacNodeId, CarrierLeg>, Position> lastCorrelationPoint_;

    // One PathLossModel strategy per carrier leg (S9b), created eagerly in
    // addRadio() when a leg's CarrierPhysics record is first established.
    // PathLossModel::owner_ is this medium, which is what relocates every
    // propagation-formula random draw onto the medium's own rng-0 stream.
    // Owned; leg records are never removed (like carrierPhysics_), so a
    // strategy lives for the run.
    std::map<CarrierLeg, PathLossModel *> pathLoss_;

    // One Tr36814PathLossModel instance per carrier leg (S14), replacing the
    // per-*endpoint* instance StochasticChannelModel::extCellPathLoss_ used
    // to own: the ext-cell/background-cell interference path always uses
    // TR 36.814 regardless of the leg's own pathLossType (computeExtCellPathLoss),
    // so every radio on a leg would build an identical instance -- the same
    // duplication pathLoss_ existed to eliminate. Draw-free (verified: neither
    // Tr36814PathLossModel.cc nor computeExtCellPathLoss() itself draws), so
    // consolidating it costs nothing in RNG attribution. Owned, like pathLoss_.
    std::map<CarrierLeg, PathLossModel *> extCellPathLoss_;

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

    /** Builds this leg's ext-cell/background-cell strategy (S14): always Tr36814PathLossModel, regardless of cp.pathLossType, from the same CarrierPhysics fields createPathLossModel() reads. */
    PathLossModel *createExtCellPathLossModel(const CarrierPhysics& cp, const CarrierLeg& leg);

    /**
     * Per-link/per-node stochastic-state accessors (plan S13/§3(b)): the
     * medium's own counterpart of the six accessors StochasticChannelModel
     * used to carry (S7/S8). Now that the containers are medium-wide rather
     * than per-endpoint, there is nothing left to redirect to a different
     * endpoint's copy, so shadowingState()/jakesState() from the S8-S12
     * shape are gone -- computeShadowing()/jakesFading() index
     * lastComputedSF_/jakesFadingMap_/jakesFadingMapBgUe_ directly.
     */

    /** Auto-vivifying access to losMap_[{leg,key}]; existed, if given, reports whether the entry was already present. */
    bool& losState(const CarrierLeg& leg, const LinkKey& key, bool *existed = nullptr);

    /**
     * Access to positionHistory_[{nodeId,leg}]. createIfMissing=true
     * auto-vivifies an empty queue like std::map::operator[]
     * (updatePositionHistory()); createIfMissing=false returns nullptr
     * instead of inserting a placeholder for a node with no history yet
     * (computeSpeed(), which must not manufacture an entry it would then
     * read as non-empty).
     */
    std::queue<Position> *positionHistory(MacNodeId nodeId, const CarrierLeg& leg, bool createIfMissing);

    /** Auto-vivifying access to lastCorrelationPoint_[{nodeId,leg}]; existed, if given, reports whether the entry was already present. */
    Position& correlationPoint(MacNodeId nodeId, const CarrierLeg& leg, bool *existed = nullptr);

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
     * The shared LOS/NLOS state for radio's own carrier leg (plan
     * S13/§3(b)): auto-vivifies to NLOS (false) if this link's LOS has
     * never been computed. Public for StochasticChannelModel's resident
     * computeExtCellPathLoss(), which -- like the primary in-cell
     * attenuation computation -- reads (and may share) this same losMap_
     * entry rather than drawing its own independent LOS state for the
     * ext-cell/background-cell interference path.
     */
    virtual bool losStateFor(StochasticChannelModel *radio, const LinkKey& key);

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
     * The per-leg ext-cell/background-cell strategy for a registered radio
     * (plan S14): the medium's counterpart of pathLossFor(), letting the
     * still-resident computeExtCellPathLoss() delegate to a shared instance
     * instead of owning its own -- extCellPathLoss_ no longer lives on the
     * endpoint.
     */
    virtual PathLossModel& extCellPathLossFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The physical-layer computation relocated from StochasticChannelModel
     * (plan step S9b): the endpoint that used to compute these now holds
     * only a one-line forwarder to here, so every random draw that used to
     * consume the endpoint's own rng-0 stream now consumes this medium's,
     * in the same order and the same count -- the byte-identical relocation
     * the step depends on. radio identifies the calling endpoint, whose own
     * O2I geometry (o2iStateOf) these read; the shared PathLossModel
     * strategy and the per-link/per-node stochastic state are both looked
     * up by radio's carrier leg (plan S13/§3(b)).
     *
     * computeShadowing()/jakesFading() lost their cqiDl parameter and
     * jakesFading() its ownerId at S13: both used to select between this
     * radio's own state and a *different*, peer endpoint's (obtainShadowingMap()/
     * obtainUeJakesMap(), deleted) -- now there is one shared entry per link
     * regardless of which end asks, so there is nothing left to redirect to.
     * computeCorrelationDistance() takes nodeId rather than a LinkKey for the
     * same reason updatePositionHistory()/updateCorrelationDistance() below
     * do: per §3(b), a node's correlation point is a property of the node
     * and its carrier leg, not of a link between two nodes.
     */
    virtual double getAttenuation(StochasticChannelModel *radio, const RadioLink& link);
    virtual double computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los);
    virtual void computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key);
    virtual double computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, double speed);
    virtual double jakesFading(StochasticChannelModel *radio, const LinkKey& key, double speed,
            unsigned int band, bool isBgUe = false);
    virtual double rayleighFading(StochasticChannelModel *radio, MacNodeId id, unsigned int band);
    virtual double computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);
    virtual double computeCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);

    /**
     * The node-motion bookkeeping relocated from StochasticChannelModel at
     * S13, alongside the positionHistory_/lastCorrelationPoint_ containers
     * themselves: both used to be resident because they read the endpoint's
     * own per-radio state directly; now that state lives here, keyed by
     * (node, CarrierLeg) (plan §3(b)), so does the bookkeeping that touches
     * it. updatePositionHistory() maintains the two-entry rolling history
     * computeSpeed() reads; updateCorrelationDistance() records the point
     * getAttenuation() measures the next call's correlationDist against.
     */
    virtual void updatePositionHistory(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);
    virtual void updateCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);

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
     * own carrier leg exactly like a cellular link's. d2dLink no longer takes
     * a useUeSideMaps flag as of S13: RadioLink lost the field it fed, since
     * a D2D link's state is the same shared entry regardless of which end asks.
     */
    virtual RadioLink d2dLink(StochasticChannelModel *radio, MacNodeId srcId, inet::Coord srcCoord,
            MacNodeId destId, inet::Coord destCoord);
    virtual std::vector<double> getReceptionSinr(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector);
    virtual void emitRcvdSinr(StochasticChannelModel *radio, Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr);
};

} //namespace

#endif
