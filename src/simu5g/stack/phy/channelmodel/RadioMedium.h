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
 * The unit CarrierPhysics and the medium's per-link stochastic state are
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
    // and it doubles as the handle onto D2D-only facts (isD2DInterferenceEnabled(),
    // the rcvdSinrD2D signal, getSINR_D2D()) that plain StochasticChannelModel
    // does not carry.
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
 * The per-carrier-leg physics parameters that every
 * radio registered on a given leg must agree on. Filled from the first radio
 * to register on that leg; every later one is checked against it,
 * field by field, in addRadio(). Per-radio parameters (antenna gains, cable
 * loss, noise figures, insideBuilding, the module-path parameters and, since
 * E4, the two antenna heights -- now a per-node RadioDescriptor::height,
 * §3(k) -- are deliberately not part of this record.
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

    // Phantom radios, keyed by the tuple that is unique --
    // real radios never enter this index, and a phantom never enters
    // radioIndex_. Same radios_ deque, same lifetime story, same
    // swap-and-pop removal; reindex() is what keeps a moved descriptor
    // pointed at from the right index.
    std::map<BgUeKey, RadioDescriptor *> bgRadioIndex_;

    // One CarrierPhysics record per carrier leg, established by the first
    // radio to register on it (see addRadio()). Nothing reads this yet.
    std::map<CarrierLeg, CarrierPhysics> carrierPhysics_;

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

    // One PathLossModel strategy per carrier leg, created eagerly in
    // addRadio() when a leg's CarrierPhysics record is first established.
    // PathLossModel::owner_ is this medium, which is what relocates every
    // propagation-formula random draw onto the medium's own rng-0 stream.
    // Owned; leg records are never removed (like carrierPhysics_), so a
    // strategy lives for the run. (E5c makes these point at legModule's own
    // submodules instead, at which point they are no longer owned.)
    std::map<CarrierLeg, PathLossModel *> pathLoss_;

    // One Tr36814PathLossModel instance per carrier leg: the
    // ext-cell/background-cell interference path always uses
    // TR 36.814 regardless of the leg's own pathLossType (computeExtCellPathLoss),
    // so every radio on a leg would otherwise build an identical instance.
    // Draw-free (verified: neither
    // Tr36814PathLossModel.cc nor computeExtCellPathLoss() itself draws), so
    // sharing it costs nothing in RNG attribution. Owned, like pathLoss_.
    std::map<CarrierLeg, PathLossModel *> extCellPathLoss_;

    // This medium's own interference submodule:
    // resolved once at initialize(), read live thereafter. Not owned -- it
    // is a child module, torn down by the module hierarchy like any other.
    CellularInterferenceModel *interference_ = nullptr;

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
     * endpoint recast E5) whose (componentCarrierModule, leg) matches
     * endpoint's own carrier leg; throws if zero or more than one match.
     */
    cModule *matchCarrierLeg(StochasticChannelModel *endpoint) const;

    /** Whether legModule's own componentCarrierModule/leg parameters admit a radio on carrierFrequency/isNr. */
    bool carrierLegMatches(cModule *legModule, GHz carrierFrequency, bool isNr) const;

    /**
     * The 3GPP study name ("Tr36814"/"Tr36873"/"Tr38901") pathLossModule's
     * own NED type encodes -- the trailing "PathLoss" stripped, matching the
     * endpoint's own "pathLossType" enum values, so a leg-sourced
     * CarrierPhysics::pathLossType compares equal to the endpoint's bridge
     * copy exactly when the two agree.
     */
    std::string pathLossStudyOf(cModule *pathLossModule) const;

    /**
     * Reads the per-carrier-leg physics parameters: 17 of the (originally 25)
     * fields come from this medium's own NED parameters (the environment,
     * stated once for the whole network -- radio endpoint recast E3); the
     * remaining 6 (pathLossType, scenario, buildingHeight, streetWidth,
     * tolerateMaxDistViolation, useBuildingPenetrationHighLossModel) come
     * from legModule -- the carrierLeg[] submodule matchCarrierLeg() matched
     * endpoint to (radio endpoint recast E5), not from endpoint's own NED
     * type any more. nodebHeight/ueHeight left this record at E4 -- antenna
     * height is a per-node fact (RadioDescriptor::height, §3(k)), not a
     * per-leg one.
     */
    CarrierPhysics readCarrierPhysics(StochasticChannelModel *endpoint, cModule *legModule) const;

    /** Checks candidate against the leg's established record; throws naming the first mismatched parameter. */
    void checkCarrierPhysics(const CarrierPhysics& existing, const CarrierPhysics& candidate,
            const CarrierLeg& leg, const std::string& candidatePath) const;

    /**
     * The ASSERT-equality bridge (move-code.md) for the parameters
     * readCarrierPhysics() now reads off this medium and off legModule: the
     * 17 environment parameters and d2dInterference (E3), plus, since E5,
     * the 6 study/geometry fields legModule now supplies -- pathLossType,
     * scenario, buildingHeight, streetWidth, tolerateMaxDistViolation and
     * (when the matched study is Tr38901) useBuildingPenetrationHighLossModel.
     * endpoint still declares its own copy of each (removal is E6), so this
     * compares that still-live value against the medium's/leg's and throws,
     * naming both module paths, the moment they disagree -- which is exactly
     * what an incorrectly migrated (or un-migrated) ini line produces.
     * Called for every registering radio, not just the first on a leg: the
     * 17+1 environment fields are network-wide, and even the 6 leg fields
     * are cheap to re-check per radio since legModule and candidate are
     * already in hand. d2dEndpoint is dynamic_cast<D2dChannelModel*>(endpoint),
     * already computed by the caller; nullptr skips the d2dInterference check.
     */
    void checkEndpointAgreesWithMedium(StochasticChannelModel *endpoint, D2dChannelModel *d2dEndpoint,
            cModule *legModule, const CarrierPhysics& candidate) const;

    /** The per-leg path-loss strategy resolved in addRadio(); throws if no radio has registered on the leg. */
    PathLossModel& pathLossFor(const CarrierLeg& leg) const;

    /** The per-leg ext-cell/background-cell path-loss strategy resolved in addRadio(); throws if no radio has registered on the leg. */
    PathLossModel& extCellPathLossFor(const CarrierLeg& leg) const;

    /** The per-leg CarrierPhysics record established in addRadio(); throws if no radio has registered on the leg. */
    const CarrierPhysics& carrierPhysicsFor(const CarrierLeg& leg) const;

    /** Builds the propagation-formula strategy matching cp.pathLossType (E5b: cp.pathLossType now derived from legModule, but still a heap `new`; E5c resolves the submodule instead) and initializes it from cp. */
    PathLossModel *createPathLossModel(const CarrierPhysics& cp);

    /** Builds this leg's ext-cell/background-cell strategy: always Tr36814PathLossModel, regardless of cp.pathLossType, from the same CarrierPhysics fields createPathLossModel() reads. */
    PathLossModel *createExtCellPathLossModel(const CarrierPhysics& cp);

    /**
     * The initialize() call createPathLossModel()/createExtCellPathLossModel()
     * share verbatim, once the concrete strategy is constructed. Since E4 this
     * carries only the leg-constant scenario parameters -- no carrier
     * frequency and no antenna heights any more (neither is per-leg state:
     * the frequency is the leg's own key already, and the heights are
     * per-node, §3(k)); both travel per call now, in a LinkContext built by
     * linkContextFor().
     */
    void initializePathLossStrategy(PathLossModel *model, const CarrierPhysics& cp);

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
    /** E5b owns the heap-built path-loss strategies; E5c hands them to the module tree and drops this. */
    ~RadioMedium() override;

    /** Resolves interference_, this medium's own interference submodule. */
    void initialize() override;

    /** Never called: this module has no gates and schedules no self-messages. */
    void handleMessage(cMessage *msg) override;

    /** Registers a radio endpoint on its carrier. Duplicate registration is an error. */
    virtual void addRadio(StochasticChannelModel *endpoint);

    /** Unregisters a radio endpoint previously added with addRadio(). */
    virtual void removeRadio(StochasticChannelModel *endpoint);

    /**
     * Registers a background transmitter's phantom radio under key, the
     * tuple that is unique: unlike addRadio(), this
     * establishes no CarrierPhysics record and creates no PathLossModel or
     * per-radio stochastic state -- a phantom declares none of the 25
     * per-carrier-leg physics parameters and owns no stochastic state.
     * height is the phantom's own antenna height (E4/§3(k)) -- the caller's
     * (BackgroundTrafficManager's own default, since no in-tree config
     * differentiates one background UE's height from another's).
     * Duplicate registration is an error, exactly as addRadio()'s is.
     */
    virtual void addBackgroundRadio(const BgUeKey& key, TrafficGeneratorBase *generator, double height);

    /** Unregisters a background transmitter's phantom radio previously added with addBackgroundRadio(). */
    virtual void removeBackgroundRadio(const BgUeKey& key);

    /**
     * The shared LOS/NLOS state for radio's own carrier leg:
     * auto-vivifies to NLOS (false) if this link's LOS has
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
    virtual LinkContext linkContextFor(StochasticChannelModel *radio, MacNodeId peerId) const;

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
    virtual double computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los, MacNodeId peerId);
    virtual void computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, MacNodeId peerId);
    virtual double computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, double speed, MacNodeId peerId);
    virtual double jakesFading(StochasticChannelModel *radio, const LinkKey& key, double speed,
            unsigned int band, bool isBgUe = false);
    virtual double rayleighFading(StochasticChannelModel *radio, MacNodeId id, unsigned int band);

    /**
     * The multipath-fading attenuation for one band, RAYLEIGH or JAKES per
     * cp.fadingType, 0 if cp.fading is off -- the body getRSRP() and
     * getSINR_bgUe() each looped over per band. Exact code motion: draws
     * from the same rng-0 stream as rayleighFading()/jakesFading()
     * themselves, in the same relative position in the caller's loop, so
     * this changes no draw's order or count.
     */
    virtual double applyFading(StochasticChannelModel *radio, const CarrierPhysics& cp, MacNodeId nodeId,
            const LinkKey& key, double speed, unsigned int band, bool isBgUe = false);

    virtual double computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);
    virtual double computeCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);

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
    virtual void updatePositionHistory(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);
    virtual void updateCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord);

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
            MacNodeId destId, inet::Coord destCoord);
    virtual std::vector<double> getReceptionSinr(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
            const std::vector<double>& rsrpVector);
    virtual void emitRcvdSinr(StochasticChannelModel *radio, Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr);
};

} //namespace

#endif
