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
 * The unit the medium's per-leg strategies and per-link stochastic state are
 * grouped by: a carrier frequency alone conflates an NR UE's
 * always-instantiated but unused LTE leg with the gNB it shares a default
 * component-carrier frequency with, and conflates a dual-connectivity master
 * eNB with its secondary gNB. Frequency stays the
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
 * Key for the medium's per-link stochastic state: the
 * carrier leg plus the unordered pair of nodes the link connects (LinkKey
 * already normalizes that pair). One entry per physical link, shared by
 * both ends: the same link draws its own LOS, shadowing and Jakes
 * realization once, not once per end.
 */
typedef std::pair<CarrierLeg, LinkKey> LinkStateKey;

typedef std::map<LinkStateKey, std::vector<JakesFadingData>> JakesFadingMap;
typedef std::map<LinkStateKey, std::pair<inet::simtime_t, double>> ShadowFadingMap;

/**
 * A background transmitter's phantom key: the tuple that *is*
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
 * the identity the registry indexes it by.
 */
struct RadioDescriptor
{
    StochasticChannelModel *endpoint = nullptr;   // null iff this is a phantom
    MacNodeId nodeId = NODEID_NONE;
    GHz carrierFrequency = GHz(0);

    // This radio's own antenna height (radio endpoint recast E4, §3(k)): a
    // per-node fact, not a per-leg constant any more. For a real endpoint,
    // its own "height" NED parameter; for a phantom, the value
    // addBackgroundRadio() was given (BackgroundTrafficManager's own
    // constant, since no in-tree config ever differentiates one background
    // UE's height from another's).
    double height = 0;

    // The D2D marker: endpoint downcast once at registration,
    // non-null iff this radio is D2D-capable. The registration-time
    // dynamic_cast this is built from is the only one -- every D2D-aware
    // branch reads this cached pointer afterward instead of casting itself,
    // and it doubles as the handle onto D2D-only facts (the rcvdSinrD2D
    // signal, getSINR_D2D()) that plain StochasticChannelModel does not
    // carry.
    D2dChannelModel *d2dEndpoint = nullptr;

    // The phantom half: non-null iff endpoint == nullptr --
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

    // Phantom radios, keyed by the tuple that is unique --
    // real radios never enter this index, and a phantom never enters
    // radioIndex_. Same radios_ deque, same lifetime story, same
    // swap-and-pop removal; reindex() is what keeps a moved descriptor
    // pointed at from the right index.
    std::map<BgUeKey, RadioDescriptor *> bgRadioIndex_;

    // Per-link stochastic state, keyed by LinkStateKey --
    // one shared entry per physical link, shared by both endpoints. losMap_ and
    // lastComputedSF_/jakesFadingMap_/jakesFadingMapBgUe_ together hold this
    // link-level LOS/shadowing/Jakes state.
    std::map<LinkStateKey, bool> losMap_;
    ShadowFadingMap lastComputedSF_;
    JakesFadingMap jakesFadingMap_;
    JakesFadingMap jakesFadingMapBgUe_;

    // Per-(node, CarrierLeg) mobility state: a node's motion
    // is a property of the node itself, not of a link between two nodes, so
    // these stay keyed one level coarser than the four containers above --
    // shared across every link that tracks the same node on the same leg.
    std::map<std::pair<MacNodeId, CarrierLeg>, std::queue<Position>> positionHistory_;
    std::map<std::pair<MacNodeId, CarrierLeg>, Position> lastCorrelationPoint_;

    // One PathLossModel strategy per carrier leg, resolved eagerly in
    // addRadio() when the first radio registers on the leg:
    // the matched carrierLeg[]'s own "pathLoss" submodule (radio endpoint
    // recast E5), not a freshly `new`-ed object any more. PathLossModel::owner_
    // is this medium, which is what relocates every propagation-formula
    // random draw onto the medium's own rng-0 stream. NOT owned: the
    // submodule is owned by the module tree (the matched carrierLeg[]
    // element), like any other submodule -- ~RadioMedium() must not delete
    // through these pointers any more.
    std::map<CarrierLeg, PathLossModel *> pathLoss_;

    // One Tr36814PathLoss submodule per carrier leg: the
    // ext-cell/background-cell interference path always uses
    // TR 36.814 regardless of the leg's own pathLossType (computeExtCellPathLoss),
    // so every radio on a leg would otherwise resolve an identical instance.
    // Draw-free (verified: neither
    // Tr36814PathLossModel.cc nor computeExtCellPathLoss() itself draws), so
    // sharing it costs nothing in RNG attribution. NOT owned, like pathLoss_.
    std::map<CarrierLeg, PathLossModel *> extCellPathLoss_;

    // This medium's own interference submodule:
    // resolved once at initialize(), read live thereafter. Not owned -- it
    // is a child module, torn down by the module hierarchy like any other.
    CellularInterferenceModel *interference_ = nullptr;

    // The environment, stated once for the whole network: this medium's own
    // NED parameters (radio endpoint recast E3), cached at initialize() and
    // read on the computation paths below. useTorus and targetBler stay
    // NED-declared but have no reader here -- neither had one on the old
    // endpoint either (targetBler's live consumer is PhyEnb's own parameter
    // of the same name).
    bool shadowing_ = false;
    double correlationDistance_ = 0;
    bool dynamicLos_ = false;
    bool fixedLos_ = false;
    bool enableExtCellLos_ = false;
    bool fading_ = false;
    std::string fadingType_;
    int numFadingPaths_ = 0;
    double delayRms_ = 0;
    double thermalNoise_ = 0;
    double harqReduction_ = 0;
    bool bgCellInterference_ = false;
    bool extCellInterference_ = false;
    bool downlinkInterference_ = false;
    bool uplinkInterference_ = false;
    bool d2dInterference_ = false;

    /**
     * Scans the active configuration for ini keys that name a parameter this
     * plan has removed or renamed -- OMNeT++ gives no diagnostic for a key
     * that matches nothing (envir has no "unused key" warning), so without
     * this a stale key is a silent physics change. Throws, naming every
     * offending key and its replacement, if any are found. Grows one entry
     * at a time as the migration removes more of the parameter surface.
     */
    void checkForLegacyConfigKeys() const;

    /** Looks up the registered radio for (nodeId, carrierFrequency); throws if none is registered. */
    const RadioDescriptor& descriptorFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /** Looks up the registered phantom for key; throws if none is registered. */
    const RadioDescriptor& descriptorFor(const BgUeKey& key) const;

    /**
     * Re-points radioIndex_ or bgRadioIndex_ (dispatching on endpoint != nullptr)
     * for descriptor, whose address just changed under swap-and-pop removal.
     * Factored so both removal paths use it: using removeRadio's own re-index
     * line unconditionally would write a phantom into the real-radio index.
     */
    void reindex(RadioDescriptor& descriptor);

    /**
     * The single carrierLeg[] submodule (of type CarrierLegPhysics, radio
     * endpoint recast E5) whose (componentCarrierModule, leg) matches the
     * (carrierFrequency, endpoint->isNr()) leg being registered; throws if
     * zero or more than one match. carrierFrequency is explicit, not
     * endpoint->getCarrierFrequency(), because since E8 one endpoint can
     * register on several carriers.
     */
    cModule *matchCarrierLeg(StochasticChannelModel *endpoint, GHz carrierFrequency) const;

    /**
     * addRadio()'s per-carrier work, factored out so the public entry point
     * can loop endpoint->getComponentCarriers() (radio endpoint recast E8,
     * §3(c)): builds and indexes one RadioDescriptor for (endpoint, carrierFrequency),
     * resolving that carrier's leg's strategies the first time any radio
     * registers on it.
     */
    void addRadioOnCarrier(StochasticChannelModel *endpoint, GHz carrierFrequency);

    /** Whether legModule's own componentCarrierModule/leg parameters admit a radio on carrierFrequency/isNr. */
    bool carrierLegMatches(cModule *legModule, GHz carrierFrequency, bool isNr) const;

    /** The per-leg path-loss strategy resolved in addRadio(); throws if no radio has registered on the leg. */
    PathLossModel& pathLossFor(const CarrierLeg& leg) const;

    /** The per-leg ext-cell/background-cell path-loss strategy resolved in addRadio(); throws if no radio has registered on the leg. */
    PathLossModel& extCellPathLossFor(const CarrierLeg& leg) const;

    /** Resolves legModule's own "pathLoss" submodule -- the leg's configured study -- and initializes it from legModule's geometry parameters. */
    PathLossModel *resolvePathLossStrategy(cModule *legModule);

    /** Resolves legModule's own "extCellPathLoss" submodule -- always Tr36814PathLoss, whatever the leg's own study -- and initializes it from the same legModule geometry resolvePathLossStrategy() reads. */
    PathLossModel *resolveExtCellPathLossStrategy(cModule *legModule);

    /**
     * The initialize() call resolvePathLossStrategy()/resolveExtCellPathLossStrategy()
     * share verbatim, once the concrete strategy is resolved. Since E4 this
     * carries only the leg-constant scenario parameters, read off legModule
     * (the deployment geometry, per-leg since E5) -- no carrier
     * frequency and no antenna heights any more (neither is per-leg state:
     * the frequency is the leg's own key already, and the heights are
     * per-node, §3(k)); both travel per call now, in a LinkContext built by
     * linkContextFor().
     */
    void initializePathLossStrategy(PathLossModel *model, cModule *legModule);

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
    /** Resolves interference_, this medium's own interference submodule. */
    void initialize() override;

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /**
     * Registers a radio endpoint on every carrier it serves
     * (endpoint->getComponentCarriers(), radio endpoint recast E8, §3(c)):
     * one endpoint module now serves a whole PHY leg rather than one carrier
     * each, so this fans out into one RadioDescriptor per carrier, all
     * sharing the same endpoint pointer. Duplicate registration (on any one
     * carrier) is an error.
     */
    virtual void addRadio(StochasticChannelModel *endpoint);

    /** Unregisters a radio endpoint previously added with addRadio(). */
    virtual void removeRadio(StochasticChannelModel *endpoint);

    /**
     * Registers a background transmitter's phantom radio under key, the
     * tuple that is unique: unlike addRadio(), this
     * resolves no carrierLeg[] entry and no PathLossModel strategy, and a
     * phantom owns no stochastic state.
     * height is the phantom's own antenna height (E4/§3(k)) -- the caller's
     * (BackgroundTrafficManager's own default, since no in-tree config
     * differentiates one background UE's height from another's).
     * Duplicate registration is an error, exactly as addRadio()'s is.
     */
    virtual void addBackgroundRadio(const BgUeKey& key, TrafficGeneratorBase *generator, double height);

    /** Unregisters a background transmitter's phantom radio previously added with addBackgroundRadio(). */
    virtual void removeBackgroundRadio(const BgUeKey& key);

    /**
     * The network-wide interference and LOS toggles, owned by the medium
     * since E3 and, since E6, no longer declared by the endpoints at all --
     * the endpoints' isUplinkInterferenceEnabled()/isD2DInterferenceEnabled()
     * overrides and the resident computeExtCellPathLoss() answer by asking
     * back here (§3(b)).
     */
    bool isUplinkInterferenceEnabled() const { return uplinkInterference_; }
    bool isD2dInterferenceEnabled() const { return d2dInterference_; }
    bool isExtCellLosEnabled() const { return enableExtCellLos_; }

    /**
     * The shared LOS/NLOS state for radio's own carrier leg:
     * auto-vivifies to NLOS (false) if this link's LOS has
     * never been computed. Public for StochasticChannelModel's resident
     * computeExtCellPathLoss(), which -- like the primary in-cell
     * attenuation computation -- reads (and may share) this same losMap_
     * entry rather than drawing its own independent LOS state for the
     * ext-cell/background-cell interference path.
     */
    virtual bool losStateFor(StochasticChannelModel *radio, const LinkKey& key, GHz carrierFrequency);

    // Physical facts of a registered radio, read live through its own
    // endpoint pointer -- the registry resolves identity, it does not cache
    // values.
    virtual inet::Coord coordOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double txPowerOf(MacNodeId nodeId, GHz carrierFrequency, Direction dir = UNKNOWN_DIRECTION) const;
    virtual TxDirectionType txDirectionOf(MacNodeId nodeId, GHz carrierFrequency) const;
    virtual double txAngleOf(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * Physical facts of a registered background transmitter:
     * the accessor family widens to the phantom key rather than forking --
     * two overloads on the existing names, not new ones. No Direction
     * overload of txPowerOf: TrafficGeneratorBase::getTxPwr() takes none, a
     * background UE has one tx power.
     */
    virtual inet::Coord coordOf(const BgUeKey& key) const;
    virtual double txPowerOf(const BgUeKey& key) const;

    /** The calling radio's outdoor-to-indoor geometry, read live off its registered endpoint. */
    virtual O2iState o2iStateOf(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The per-*call* facts (carrier frequency triple, the two antenna
     * heights) a propagation-formula call needs, resolved for the specific
     * link between radio and peerId (radio endpoint recast E4, §3(f),(k)).
     * Public, like the rest of this accessor family: StochasticChannelModel's
     * resident computeExtCellPathLoss() calls it back on the medium too, the
     * same way it already reaches extCellPathLossFor()/losStateFor().
     *
     * The frequency triple is radio's own carrier frequency, reproducing
     * ChannelModelBase's own derivation (ChannelModelBase.cc:26-29) with no
     * per-leg cache any more.
     *
     * Heights are resolved by *role*, not by which end is transmitting or
     * receiving (risk 15): whichever of {radio, peerId} is eNB-role
     * (getNodeTypeById()) supplies h_BS, whichever is UE-role supplies h_UT.
     * radio's own role and height are read directly off its own registered
     * radio; peerId's are looked up by id -- a real registered radio first,
     * then (num(peerId) >= BGUE_MIN_ID) a phantom background UE, keyed off
     * radio's own node id as the owning cell (valid because radio is always
     * that phantom's serving eNB whenever it appears as a peer here --
     * BackgroundTrafficManager registers a phantom under its own eNB's node
     * id as cellId, LteMacEnb.cc:131).
     *
     * peerId == NODEID_NONE -- no specific peer identity reaches the call
     * (getReceivedPower_bgUe(), the D2D conflict-graph's abstract distance
     * estimate, computeExtCellPathLoss()'s ext-cell/background-cell walk,
     * none of which identify a specific far-end node) -- falls back to the
     * *other* role's own NIC-level default height (25m eNB-side, 1.5m
     * UE-side) for the missing side. Value-preserving today by construction,
     * not by guess: §3(k) verifies every in-tree config keeps that side at
     * its role's default in exactly these calls' configs.
     */
    virtual LinkContext linkContextFor(StochasticChannelModel *radio, MacNodeId peerId, GHz carrierFrequency) const;

    /**
     * The per-leg path-loss strategy for a registered radio, resolved through
     * the registry like the rest of this accessor family. Lets the
     * endpoint's own computeAngularAttenuation -- whose formula never draws,
     * so it stays resident on the endpoint -- keep delegating to the strategy,
     * which pathLoss_ holds on the medium, not the endpoint.
     */
    virtual PathLossModel& pathLossFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The per-leg ext-cell/background-cell strategy for a registered radio:
     * the medium's counterpart of pathLossFor(), letting the
     * resident computeExtCellPathLoss() delegate to a shared instance
     * instead of owning its own -- extCellPathLoss_ lives on the medium, not
     * the endpoint.
     */
    virtual PathLossModel& extCellPathLossFor(MacNodeId nodeId, GHz carrierFrequency) const;

    /**
     * The physical-layer computation, resident on the medium: the endpoint
     * holds only a one-line forwarder to here, so every random draw
     * consumes this medium's rng-0 stream, in the same order and the same
     * count for every endpoint. radio identifies the calling endpoint, whose own
     * O2I geometry (o2iStateOf) these read; the shared PathLossModel
     * strategy and the per-link/per-node stochastic state are both looked
     * up by radio's carrier leg.
     *
     * computeShadowing()/jakesFading() take no cqiDl parameter and
     * jakesFading() no ownerId: there is one shared entry per link,
     * regardless of which end asks, so there is nothing to redirect to.
     * computeCorrelationDistance() takes nodeId rather than a LinkKey for the
     * same reason updatePositionHistory()/updateCorrelationDistance() below
     * do: a node's correlation point is a property of the node
     * and its carrier leg, not of a link between two nodes.
     *
     * peerId (E4/§3(k)): the other party of this specific link, for
     * per-node height role-resolution (linkContextFor()) -- NODEID_NONE where
     * no specific peer identity reaches the call (getReceivedPower_bgUe(),
     * the D2D conflict-graph's abstract distance estimate), which
     * linkContextFor() resolves to the UE-side default height.
     */
    virtual double getAttenuation(StochasticChannelModel *radio, const RadioLink& link);
    virtual double computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los, MacNodeId peerId, GHz carrierFrequency);
    virtual void computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, MacNodeId peerId, GHz carrierFrequency);
    virtual double computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, double speed, MacNodeId peerId, GHz carrierFrequency);
    virtual double jakesFading(StochasticChannelModel *radio, const LinkKey& key, double speed,
            unsigned int band, GHz carrierFrequency, bool isBgUe = false);
    virtual double rayleighFading(StochasticChannelModel *radio, MacNodeId id, unsigned int band);

    /**
     * The multipath-fading attenuation for one band, RAYLEIGH or JAKES per
     * fadingType_, 0 if fading_ is off -- the body getRSRP() and
     * getSINR_bgUe() each looped over per band. Exact code motion: draws
     * from the same rng-0 stream as rayleighFading()/jakesFading()
     * themselves, in the same relative position in the caller's loop, so
     * this changes no draw's order or count.
     */
    virtual double applyFading(StochasticChannelModel *radio, MacNodeId nodeId,
            const LinkKey& key, double speed, unsigned int band, GHz carrierFrequency, bool isBgUe = false);

    virtual double computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord, GHz carrierFrequency);
    virtual double computeCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord, GHz carrierFrequency);

    /**
     * Attenuation from a sectorial (ANISOTROPIC) transmitter's antenna
     * pattern, 0 for an OMNI one -- the shape shared by getRSRP(),
     * getSINR_bgUe(), getReceivedPower_bgUe() and
     * CellularInterferenceModel's computeDownlinkInterference(), which
     * otherwise duplicated this body once each. Draws nothing: txId's
     * direction/angle come from the registry (txDirectionOf/txAngleOf),
     * not a fresh draw, and the angle geometry itself
     * (radio->computeAngle/computeVerticalAngle/computeAngularAttenuation)
     * is a deterministic formula.
     */
    virtual double computeSectorAttenuation(StochasticChannelModel *radio, MacNodeId txId, GHz carrierFrequency,
            const inet::Coord& txCoord, const inet::Coord& rxCoord) const;

    /**
     * The node-motion bookkeeping, resident on the medium alongside the
     * positionHistory_/lastCorrelationPoint_ containers themselves, keyed by
     * (node, CarrierLeg). updatePositionHistory() maintains the two-entry rolling history
     * computeSpeed() reads; updateCorrelationDistance() records the point
     * getAttenuation() measures the next call's correlationDist against.
     */
    virtual void updatePositionHistory(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord, GHz carrierFrequency);
    virtual void updateCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord, GHz carrierFrequency);

    /**
     * The SINR/RSRP/reception-decision surface, resident on the medium:
     * same one-line-forwarder shape
     * as the physical-layer computation above. isReceptionSuccessful's BLER draw
     * (uniform(0.0, 1.0)) draws from this medium's rng-0 stream.
     * computeInterferencePlusNoise calls interference_
     * (this medium's own submodule), never radio, for the four cellular
     * walks and for the D2D walk too.
     */
    virtual std::vector<double> getSINR(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo);
    virtual std::vector<double> getSINR(StochasticChannelModel *radio, const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector);
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
     * D2D-aware branches, resident on the medium:
     * D2dChannelModel is an endpoint marker (RadioDescriptor::d2dEndpoint),
     * and getReceptionSinr/emitRcvdSinr/computeInterferencePlusNoise are plain
     * sibling methods here, not virtual dispatch points, so
     * isReceptionSuccessful/getSINR call them directly
     * instead of through radio. d2dLink is the link-builder counterpart of
     * linkFor/cellularLink; computeD2DInterference is
     * interference-walk-shaped and lives beside its four cellular siblings on
     * interference_. Neither draws; the only random draw
     * a D2D reception depends on is getAttenuation's, reached through radio's
     * own carrier leg exactly like a cellular link's. d2dLink takes no
     * useUeSideMaps flag: RadioLink carries no such field, since
     * a D2D link's state is the same shared entry regardless of which end asks.
     */
    virtual RadioLink d2dLink(StochasticChannelModel *radio, MacNodeId srcId, inet::Coord srcCoord,
            MacNodeId destId, inet::Coord destCoord, GHz carrierFrequency);
    virtual std::vector<double> getReceptionSinr(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector);
    virtual void emitRcvdSinr(StochasticChannelModel *radio, Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr);
};

} //namespace

#endif
