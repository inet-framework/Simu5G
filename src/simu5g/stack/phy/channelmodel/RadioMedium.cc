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
#include <cmath>

#include <inet/common/INETDefs.h>

#include "simu5g/background/trafficGenerator/generators/TrafficGeneratorBase.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/carrierAggregation/ComponentCarrier.h"
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/d2d/phy/channelmodel/D2dChannelModel.h"
#include "simu5g/stack/mac/amc/UserTxParams.h"
#include "simu5g/stack/phy/PhyUe.h"
#include "simu5g/stack/phy/channelmodel/CellularInterferenceModel.h"
#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"
#include "simu5g/stack/phy/channelmodel/Tr38901PathLossModel.h"

namespace simu5g {

Define_Module(RadioMedium);

namespace {

/** The carrier leg a registered endpoint belongs to: its own carrier frequency plus its isNr flag. */
CarrierLeg legFor(StochasticChannelModel *endpoint)
{
    return CarrierLeg{endpoint->getCarrierFrequency(), endpoint->isNr()};
}

} // namespace

void RadioMedium::initialize()
{
    checkForLegacyConfigKeys();

    // the environment, stated once for the whole network (E3): cached off
    // this medium's own NED parameters, read on the computation paths below
    shadowing_ = par("shadowing");
    correlationDistance_ = par("correlationDistance");
    dynamicLos_ = par("dynamicLos");
    fixedLos_ = par("fixedLos");
    enableExtCellLos_ = par("enableExtCellLos");
    fading_ = par("fading");
    fadingType_ = par("fadingType").stringValue();
    // fail fast at init: applyFading()'s fadingType dispatch has no else
    // branch, so a misconfigured value would silently draw no fading at all
    if (fadingType_ != "JAKES" && fadingType_ != "RAYLEIGH")
        throw cRuntimeError("Unrecognized value in 'fadingType' parameter: \"%s\"", fadingType_.c_str());
    numFadingPaths_ = par("numFadingPaths");
    delayRms_ = par("delayRms");
    thermalNoise_ = par("thermalNoise");
    harqReduction_ = par("harqReduction");
    bgCellInterference_ = par("bgCellInterference");
    extCellInterference_ = par("extCellInterference");
    downlinkInterference_ = par("downlinkInterference");
    uplinkInterference_ = par("uplinkInterference");
    d2dInterference_ = par("d2dInterference");

    // this medium's own submodule; purely structural, so
    // resolvable regardless of init-stage ordering
    interference_ = check_and_cast<CellularInterferenceModel *>(getSubmodule("interference"));
}

void RadioMedium::checkForLegacyConfigKeys() const
{
    // Ini-key name -> what to tell a reader who still sets it. Grown one
    // entry per removal/rename as the radio-endpoint recast proceeds; see
    // the design doc, "the legacy-key guard".
    static const std::vector<std::pair<std::string, std::string>> legacyNames = {
        { "antennGainMicro", "removed from the radio endpoint; still live on BackgroundCellChannelModel" },
        // E4: unlike the 17 environment parameters E3 moved (still valid
        // under **.X, since RadioMedium re-declares the same name), these two
        // are gone from every module -- replaced by the per-node "height",
        // not merely relocated -- so they qualify for the name rule already
        // (§3(g)), ahead of the plan's own general E6/E8/E9 growth schedule.
        { "nodebHeight", "replaced by the per-node 'height' on the radio endpoint (eNB/gNB-role)" },
        { "ueHeight", "replaced by the per-node 'height' on the radio endpoint (UE-role)" },
        // E6: the study is selected on the medium now, per carrier leg
        { "pathLossType", "removed -- select the study on the medium instead: "
                          "radioMedium.carrierLeg[*].pathLoss.typename = \"Tr36814PathLoss\"/\"Tr36873PathLoss\"/\"Tr38901PathLoss\"" },
    };

    // Stale ini *values* (S8: matched as values, not key names): the two
    // preset NED types died with the endpoint's pathLossType parameter at
    // E6, so any key still selecting one -- channelModelType,
    // nrChannelModelType -- silently instantiates nothing.
    static const std::vector<std::pair<std::string, std::string>> legacyValues = {
        { "Tr36873ChannelModel", "this NED type is gone -- use \"StochasticChannelModel\" and select the study on the medium: "
                                 "radioMedium.carrierLeg[*].pathLoss.typename = \"Tr36873PathLoss\"" },
        { "Tr38901ChannelModel", "this NED type is gone -- use \"StochasticChannelModel\" and select the study on the medium: "
                                 "radioMedium.carrierLeg[*].pathLoss.typename = \"Tr38901PathLoss\"" },
    };

    // Both flags are required: a per-object config option such as 'rng-0'
    // is classified by FILT_PER_OBJECT_CONFIG, not FILT_PARAM, because its
    // key's last path segment contains a hyphen (sectionbasedconfig.cc).
    std::vector<const char *> pairs = getEnvir()->getConfigEx()->getKeyValuePairs(
            cConfigurationEx::FILT_PARAM | cConfigurationEx::FILT_PER_OBJECT_CONFIG);

    std::string offenders;
    for (size_t i = 0; i < pairs.size(); i += 2) {
        std::string key = pairs[i];
        size_t lastDot = key.find_last_of('.');
        std::string lastSegment = (lastDot == std::string::npos) ? key : key.substr(lastDot + 1);
        for (const auto& [name, replacement] : legacyNames) {
            if (lastSegment == name) {
                offenders += "\n  " + key + "\n    " + name + ": " + replacement;
                break;
            }
        }
        std::string value = pairs[i + 1];
        for (const auto& [name, replacement] : legacyValues) {
            // substring, not equality: the raw config value carries its quotes
            if (value.find(name) != std::string::npos) {
                offenders += "\n  " + key + " = " + value + "\n    " + name + ": " + replacement;
                break;
            }
        }
    }

    if (!offenders.empty())
        throw cRuntimeError("stale configuration key(s), no longer effective:%s", offenders.c_str());
}

void RadioMedium::handleMessage(cMessage *msg)
{
    throw cRuntimeError("unexpected message '%s': RadioMedium has no gates and schedules no self-messages", msg->getName());
}

bool RadioMedium::carrierLegMatches(cModule *legModule, GHz carrierFrequency, bool isNr) const
{
    std::string legStr = legModule->par("leg").stringValue();
    if (legStr != "any" && legStr != (isNr ? "NR" : "LTE"))
        return false;

    std::string ccPath = legModule->par("componentCarrierModule").stringValue();
    if (ccPath.empty())
        return true;   // "" -- matches any carrier frequency

    auto *cc = check_and_cast<ComponentCarrier *>(legModule->getModuleByPath(ccPath.c_str()));
    return cc->getCarrierFrequency() == carrierFrequency;
}

cModule *RadioMedium::matchCarrierLeg(StochasticChannelModel *endpoint) const
{
    GHz freq = endpoint->getCarrierFrequency();
    bool isNr = endpoint->isNr();

    cModule *match = nullptr;
    int numLegs = getSubmoduleVectorSize("carrierLeg");
    for (int i = 0; i < numLegs; i++) {
        cModule *legModule = getSubmodule("carrierLeg", i);
        if (carrierLegMatches(legModule, freq, isNr)) {
            if (match != nullptr)
                throw cRuntimeError("ambiguous carrierLeg match for leg %gGHz/%s: both '%s' and '%s' match",
                        freq.get(), isNr ? "NR" : "LTE", match->getFullPath().c_str(), legModule->getFullPath().c_str());
            match = legModule;
        }
    }
    if (match == nullptr)
        throw cRuntimeError("no carrierLeg entry configures leg %gGHz/%s", freq.get(), isNr ? "NR" : "LTE");
    return match;
}

void RadioMedium::addRadio(StochasticChannelModel *endpoint)
{
    ASSERT(endpoint != nullptr);

    RadioDescriptor descriptor;
    descriptor.endpoint = endpoint;
    descriptor.nodeId = endpoint->getNodeId();
    descriptor.carrierFrequency = endpoint->getCarrierFrequency();
    descriptor.height = endpoint->getHeight();
    // The D2D marker: one dynamic_cast, here, cached for every
    // D2D-aware branch to read afterward instead of casting itself.
    descriptor.d2dEndpoint = dynamic_cast<D2dChannelModel *>(endpoint);
    // exactly one of endpoint/bgGenerator is ever non-null

    auto key = std::make_pair(descriptor.nodeId, descriptor.carrierFrequency);
    if (radioIndex_.find(key) != radioIndex_.end())
        throw cRuntimeError("addRadio: node %d is already registered on carrier %gGHz",
                num(descriptor.nodeId), descriptor.carrierFrequency.get());

    // resolve this carrier leg's two strategies from the matched carrierLeg[]
    // submodule the first time a radio registers on the leg. Later
    // registrants cannot disagree with the first any more (E6): every physics
    // value is read off that same leg submodule and off this medium's own
    // parameters, never off the registering radio. (The leg, not the bare
    // frequency: frequency alone conflates an NR UE's vestigial LTE leg with
    // the gNB it shares a default component carrier with, and a
    // dual-connectivity master eNB with its secondary gNB.)
    CarrierLeg leg = legFor(endpoint);
    cModule *legModule = matchCarrierLeg(endpoint);
    if (pathLoss_.find(leg) == pathLoss_.end()) {
        // this leg's path-loss strategy: legModule's own "pathLoss"
        // submodule, resolved (not constructed) -- never from this radio's
        // own members
        pathLoss_[leg] = resolvePathLossStrategy(legModule);

        // this leg's ext-cell/background-cell strategy, shared by every
        // radio on the leg
        extCellPathLoss_[leg] = resolveExtCellPathLossStrategy(legModule);
    }

    radios_.push_back(descriptor);
    radioIndex_[key] = &radios_.back();
}

void RadioMedium::addBackgroundRadio(const BgUeKey& key, TrafficGeneratorBase *generator, double height)
{
    ASSERT(generator != nullptr);

    if (bgRadioIndex_.find(key) != bgRadioIndex_.end())
        throw cRuntimeError("addBackgroundRadio: background UE %d of cell %d is already registered on carrier %gGHz",
                num(key.bgUeId), num(key.cellId), key.carrierFrequency.get());

    RadioDescriptor descriptor;
    descriptor.bgGenerator = generator;
    descriptor.nodeId = key.bgUeId;
    descriptor.carrierFrequency = key.carrierFrequency;
    descriptor.bgCellId = key.cellId;
    descriptor.height = height;
    // exactly one of endpoint/bgGenerator is ever non-null

    // no matchCarrierLeg, no resolvePathLossStrategy: a phantom
    // is never radio in getAttenuation()/computeShadowing()/
    // jakesFading(), so it never keys an entry into the per-link state either
    radios_.push_back(descriptor);
    bgRadioIndex_[key] = &radios_.back();
}

void RadioMedium::removeBackgroundRadio(const BgUeKey& key)
{
    auto it = std::find_if(radios_.begin(), radios_.end(),
            [&key](const RadioDescriptor& d) {
                return d.bgGenerator != nullptr && d.bgCellId == key.cellId
                    && d.carrierFrequency == key.carrierFrequency && d.nodeId == key.bgUeId;
            });
    if (it == radios_.end())
        throw cRuntimeError("removeBackgroundRadio: background radio was never registered with this medium");

    bgRadioIndex_.erase(key);

    // swap-and-pop, mirroring removeRadio()
    size_t idx = it - radios_.begin();
    radios_[idx] = radios_.back();
    radios_.pop_back();
    if (idx < radios_.size())
        reindex(radios_[idx]);
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
    // re-points whichever index entry (radioIndex_ or bgRadioIndex_) held
    // the descriptor that took the removed one's place (if the removed one
    // was not already the last) -- reindex() dispatches on which one
    // (unconditionally re-indexing into
    // radioIndex_ here would write a phantom into the real-radio index)
    size_t idx = it - radios_.begin();
    radios_[idx] = radios_.back();
    radios_.pop_back();
    if (idx < radios_.size())
        reindex(radios_[idx]);
}

void RadioMedium::reindex(RadioDescriptor& descriptor)
{
    if (descriptor.endpoint != nullptr)
        radioIndex_[std::make_pair(descriptor.nodeId, descriptor.carrierFrequency)] = &descriptor;
    else
        bgRadioIndex_[BgUeKey{descriptor.bgCellId, descriptor.carrierFrequency, descriptor.nodeId}] = &descriptor;
}

const RadioDescriptor& RadioMedium::descriptorFor(MacNodeId nodeId, GHz carrierFrequency) const
{
    auto it = radioIndex_.find(std::make_pair(nodeId, carrierFrequency));
    if (it == radioIndex_.end())
        throw cRuntimeError("no radio registered for node %d on carrier %gGHz", num(nodeId), carrierFrequency.get());
    return *it->second;
}

const RadioDescriptor& RadioMedium::descriptorFor(const BgUeKey& key) const
{
    auto it = bgRadioIndex_.find(key);
    if (it == bgRadioIndex_.end())
        throw cRuntimeError("no background radio registered for cell %d, bgUe %d on carrier %gGHz",
                num(key.cellId), num(key.bgUeId), key.carrierFrequency.get());
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

inet::Coord RadioMedium::coordOf(const BgUeKey& key) const
{
    return descriptorFor(key).bgGenerator->getCoord();
}

double RadioMedium::txPowerOf(const BgUeKey& key) const
{
    return descriptorFor(key).bgGenerator->getTxPwr();
}

TxDirectionType RadioMedium::txDirectionOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getTxDirection();
}

double RadioMedium::txAngleOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    return descriptorFor(nodeId, carrierFrequency).endpoint->getTxAngle();
}

O2iState RadioMedium::o2iStateOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    const RadioDescriptor& d = descriptorFor(nodeId, carrierFrequency);
    return O2iState{d.endpoint->getInsideBuilding(), d.endpoint->getInsideDistance()};
}

namespace {

// NIC-level default heights (LteNicEnb.ned/LteNicUe.ned, StochasticChannelModel.ned):
// the fallback for linkContextFor()'s missing side when no specific peer
// identity reaches the call. Every in-tree config that reaches that fallback
// keeps the missing side at exactly this default (§3(k)), so the constant
// reproduces today's value rather than guessing at it.
constexpr double DEFAULT_ENB_HEIGHT_M = 25;
constexpr double DEFAULT_UE_HEIGHT_M = 1.5;

} // namespace

LinkContext RadioMedium::linkContextFor(StochasticChannelModel *radio, MacNodeId peerId) const
{
    LinkContext link;

    // frequency triple: radio's own carrier frequency, reproducing
    // ChannelModelBase's own derivation (ChannelModelBase.cc:26-29) -- no
    // per-leg cache any more (E4/§3(f))
    GHz freq = radio->getCarrierFrequency();
    link.carrierFrequencyGHz = GHz(freq).get();
    link.carrierFrequencyHz = Hz(freq).get();
    link.log10CarrierFrequencyGHz = log10(link.carrierFrequencyGHz);

    // heights: role-based, not tx/rx-based (risk 15)
    RanNodeType radioRole = getNodeTypeById(radio->getNodeId());
    ASSERT(radioRole == NODEB || radioRole == UE);
    double radioHeight = radio->getHeight();

    double peerHeight;
    if (peerId == NODEID_NONE) {
        // no specific peer identity reaches this call (getReceivedPower_bgUe(),
        // the D2D conflict-graph's abstract distance estimate,
        // computeExtCellPathLoss()'s ext-cell/background-cell walk) -- the
        // missing side falls back to the *other* role's own NIC-level default.
        peerHeight = (radioRole == NODEB) ? DEFAULT_UE_HEIGHT_M : DEFAULT_ENB_HEIGHT_M;
    }
    else if (num(peerId) < BGUE_MIN_ID) {
        // a real registered radio
        peerHeight = descriptorFor(peerId, freq).height;
    }
    else {
        // a phantom background UE, owned by radio's own cell -- radio is
        // always that phantom's serving eNB whenever it appears as a peer
        // here (BackgroundTrafficManager registers it under its own eNB's
        // node id as cellId, LteMacEnb.cc:131)
        ASSERT(radioRole == NODEB);
        peerHeight = descriptorFor(BgUeKey{radio->getNodeId(), freq, peerId}).height;
    }

    if (radioRole == NODEB) {
        link.hNodeB = radioHeight;
        link.hUe = peerHeight;
    }
    else {
        link.hNodeB = peerHeight;
        link.hUe = radioHeight;
    }
    return link;
}

PathLossModel& RadioMedium::pathLossFor(const CarrierLeg& leg) const
{
    auto it = pathLoss_.find(leg);
    if (it == pathLoss_.end())
        throw cRuntimeError("no path-loss strategy for carrier leg %gGHz/%s",
                leg.carrierFrequency.get(), leg.isNr ? "NR" : "LTE");
    return *it->second;
}

PathLossModel& RadioMedium::pathLossFor(MacNodeId nodeId, GHz carrierFrequency) const
{
    const RadioDescriptor& d = descriptorFor(nodeId, carrierFrequency);
    return pathLossFor(legFor(d.endpoint));
}

PathLossModel& RadioMedium::extCellPathLossFor(const CarrierLeg& leg) const
{
    auto it = extCellPathLoss_.find(leg);
    if (it == extCellPathLoss_.end())
        throw cRuntimeError("no ext-cell path-loss strategy for carrier leg %gGHz/%s",
                leg.carrierFrequency.get(), leg.isNr ? "NR" : "LTE");
    return *it->second;
}

PathLossModel& RadioMedium::extCellPathLossFor(MacNodeId nodeId, GHz carrierFrequency) const
{
    const RadioDescriptor& d = descriptorFor(nodeId, carrierFrequency);
    return extCellPathLossFor(legFor(d.endpoint));
}

PathLossModel *RadioMedium::resolvePathLossStrategy(cModule *legModule)
{
    // legModule's own "pathLoss" submodule: its concrete NED type IS the
    // leg's configured study, so there is no type-name dispatch left to do
    // here -- only the Tr38901-specific setter call, whose parameter lives
    // on the Tr38901PathLoss NED type alone (reading it off any other
    // concrete type would throw "no such parameter").
    cModule *pathLossModule = legModule->getSubmodule("pathLoss");
    auto *model = check_and_cast<PathLossModel *>(pathLossModule);

    if (auto *tr38901 = dynamic_cast<Tr38901PathLossModel *>(model))
        tr38901->setUseBuildingPenetrationHighLossModel(pathLossModule->par("useBuildingPenetrationHighLossModel"));

    initializePathLossStrategy(model, legModule);
    return model;
}

PathLossModel *RadioMedium::resolveExtCellPathLossStrategy(cModule *legModule)
{
    // always TR 36.814, whatever the leg's own study: the ext-cell and
    // background-cell interference paths (computeExtCellPathLoss) use those
    // formulas unconditionally -- CarrierLegPhysics.extCellPathLoss's own
    // default typename enforces this, not a choice made here
    auto *model = check_and_cast<PathLossModel *>(legModule->getSubmodule("extCellPathLoss"));

    initializePathLossStrategy(model, legModule);
    return model;
}

void RadioMedium::initializePathLossStrategy(PathLossModel *model, cModule *legModule)
{
    // the leg-constant deployment geometry, legModule's own NED parameters
    // (E5/E6). No carrier frequency, no antenna heights any more (E4): those
    // travel per call now, in a LinkContext built by linkContextFor()
    model->initialize(this, aToDeploymentScenario(legModule->par("scenario").stringValue()),
            legModule->par("buildingHeight"), legModule->par("streetWidth"),
            legModule->par("tolerateMaxDistViolation"));
}

bool& RadioMedium::losState(const CarrierLeg& leg, const LinkKey& key, bool *existed)
{
    auto result = losMap_.try_emplace(LinkStateKey{leg, key}, false);
    if (existed != nullptr)
        *existed = !result.second;
    return result.first->second;
}

std::queue<Position> *RadioMedium::positionHistory(MacNodeId nodeId, const CarrierLeg& leg, bool createIfMissing)
{
    auto key = std::make_pair(nodeId, leg);
    if (createIfMissing)
        return &positionHistory_[key];
    auto it = positionHistory_.find(key);
    return it == positionHistory_.end() ? nullptr : &it->second;
}

Position& RadioMedium::correlationPoint(MacNodeId nodeId, const CarrierLeg& leg, bool *existed)
{
    auto result = lastCorrelationPoint_.try_emplace(std::make_pair(nodeId, leg));
    if (existed != nullptr)
        *existed = !result.second;
    return result.first->second;
}

bool RadioMedium::losStateFor(StochasticChannelModel *radio, const LinkKey& key)
{
    return losState(legFor(radio), key);
}

void RadioMedium::updatePositionHistory(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord)
{
    // createIfMissing=true: a freshly vivified (empty) queue makes
    // "!history->empty()" false, exactly like the old find()==end() check
    std::queue<Position> *history = positionHistory(nodeId, legFor(radio), true);

    if (!history->empty() && history->back().first == NOW)
        // position already updated for this TTI.
        return;

    // FIXME: possible memory leak
    history->push(Position(NOW, coord));

    if (history->size() > 2) // if we have more than a past and a current element
        // drop the oldest one
        history->pop();
}

void RadioMedium::updateCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord)
{
    const CarrierLeg leg = legFor(radio);
    bool existed = false;
    Position& point = correlationPoint(nodeId, leg, &existed);

    if (!existed) {
        // no lastCorrelationPoint set current point.
        point = Position(NOW, coord);
    }
    else if ((point.first != NOW) &&
             point.second.distance(coord) > correlationDistance_)
    {
        // check simtime_t first
        point = Position(NOW, coord);
    }
}

double RadioMedium::getAttenuation(StochasticChannelModel *radio, const RadioLink& link)
{
    double threeDimDistance = link.txCoord.distance(link.rxCoord);
    double twoDimDistance = radio->getTwoDimDistance(link.txCoord, link.rxCoord);

    double speed = computeSpeed(radio, link.stateNodeId, link.stateCoord);
    double correlationDist = computeCorrelationDistance(radio, link.stateNodeId, link.stateCoord);

    const CarrierLeg leg = legFor(radio);

    // The other party of this link, for height role-resolution (E4/§3(k),
    // risk 15): whichever of {radio, peerId} is eNB-role supplies h_BS,
    // whichever is UE-role supplies h_UT (linkContextFor()). radio's own
    // role is intrinsic to its own node id; the peer is stateNodeId (always
    // the UE, cellularLink()/linkFor()) when radio is eNB-role, or cellId
    // (always the serving eNB, linkFor()/d2dLink()) when radio is UE-role --
    // radio is never the eNB-role party of a D2D link, so this reduces
    // correctly there too.
    MacNodeId peerId = (getNodeTypeById(radio->getNodeId()) == NODEB) ? link.stateNodeId : link.cellId;

    // If Euclidean distance since last LOS probability computation is greater than
    // correlation distance the UE could have changed its state and
    // its visibility from eNodeB, hence it is correct to recompute the LOS probability
    bool losAlreadyComputed = false;
    losState(leg, link.stateKey, &losAlreadyComputed);
    if (correlationDist > correlationDistance_ || !losAlreadyComputed) {
        computeLosProbability(radio, threeDimDistance, twoDimDistance, link.stateKey, peerId);
    }

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losState(leg, link.stateKey);
    double attenuation = computePathLoss(radio, threeDimDistance, twoDimDistance, los, peerId);

    //    Applying shadowing only if it is enabled by configuration
    //    log-normal shadowing (not available for background UEs)
    if (num(link.stateNodeId) < BGUE_MIN_ID && shadowing_)
        attenuation += computeShadowing(radio, threeDimDistance, twoDimDistance, link.stateKey, speed, peerId);

    // update the tracked node's current position
    updatePositionHistory(radio, link.stateNodeId, link.stateCoord);
    updateCorrelationDistance(radio, link.stateNodeId, link.stateCoord);

    EV << "RadioMedium::getAttenuation - computed attenuation at distance " << threeDimDistance << " for eNB is " << attenuation << endl;

    return attenuation;
}

double RadioMedium::computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los, MacNodeId peerId)
{
    O2iState o2i = o2iStateOf(radio->getNodeId(), radio->getCarrierFrequency());
    return pathLossFor(legFor(radio)).computePathLoss(distance, dbp, los, o2i, linkContextFor(radio, peerId));
}

void RadioMedium::computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, MacNodeId peerId)
{
    const CarrierLeg leg = legFor(radio);

    if (!dynamicLos_) {
        losState(leg, key) = fixedLos_;
        return;
    }
    double p = pathLossFor(leg).computeLosProbability(d3D, d2D, linkContextFor(radio, peerId));
    losState(leg, key) = (uniform(0.0, 1.0) <= p);
}

double RadioMedium::computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key, double speed, MacNodeId peerId)
{
    const CarrierLeg leg = legFor(radio);
    const LinkStateKey stateKey{leg, key};

    double mean = 0;

    // Get std deviation according to LOS/NLOS and selected scenario
    double stdDev = pathLossFor(leg).getShadowingStdDev(d3D, d2D, losState(leg, key), linkContextFor(radio, peerId));
    double time = 0;
    double space = 0;
    double att;

    // if shadowing for current link has never been computed
    if (lastComputedSF_.find(stateKey) == lastComputedSF_.end()) {
        //Get the log-normal shadowing with std deviation stdDev
        att = normal(mean, stdDev);

        //store the shadowing attenuation for this link and the temporal mark
        std::pair<simtime_t, double> tmp(NOW, att);
        lastComputedSF_[stateKey] = tmp;

        //If the shadowing attenuation has been computed at least one time for this link
        // and the distance traveled by the UE is greater than correlation distance
    }
    else if ((NOW - lastComputedSF_.at(stateKey).first).dbl() * speed > correlationDistance_) {

        //get the temporal mark of the last computed shadowing attenuation
        time = (NOW - lastComputedSF_.at(stateKey).first).dbl();

        //compute the traveled distance
        space = time * speed;

        //Compute shadowing with an EAW (Exponential Average Window) (step 1)
        double a = exp(-0.5 * (space / correlationDistance_));

        //Get last shadowing attenuation computed
        double old = lastComputedSF_.at(stateKey).second;

        //Compute shadowing with an EAW (Exponential Average Window) (step 2)
        att = a * old + sqrt(1 - pow(a, 2)) * normal(mean, stdDev);

        // Store the new computed shadowing
        std::pair<simtime_t, double> tmp(NOW, att);
        lastComputedSF_[stateKey] = tmp;

        // if the distance traveled by the UE is smaller than correlation distance shadowing attenuation remains the same
    }
    else {
        att = lastComputedSF_.at(stateKey).second;
    }

    return att;
}

double RadioMedium::jakesFading(StochasticChannelModel *radio, const LinkKey& key, double speed,
        unsigned int band, bool isBgUe)
{
    // isBgUe selects the background-UE Jakes twin: the two
    // maps are otherwise identical in shape and semantics, kept separate
    // only so a background UE's key range (>= BGUE_MIN_ID) never shares an
    // entry with a real UE's, even coincidentally.
    JakesFadingMap& actualJakesMap = isBgUe ? jakesFadingMapBgUe_ : jakesFadingMap_;

    const CarrierLeg leg = legFor(radio);
    const LinkStateKey stateKey{leg, key};

    // if this is the first time that we compute fading for current link
    if (actualJakesMap.find(stateKey) == actualJakesMap.end()) {
        // clear the map
        // FIXME: possible memory leak
        actualJakesMap[stateKey].clear();

        // for each band we are going to create a Jakes fading
        for (unsigned int j = 0; j < radio->getNumBands(radio->getCarrierFrequency()); j++) {
            // clear some structure
            JakesFadingData temp;
            temp.angleOfArrival.clear();
            temp.delaySpread.clear();

            // for each fading path
            for (int i = 0; i < numFadingPaths_; i++) {
                // get angle of arrivals
                temp.angleOfArrival.push_back(cos(uniform(0, M_PI)));

                // get delay spread
                temp.delaySpread.push_back(exponential(delayRms_));
            }
            // store the Jakes fading for this link
            actualJakesMap[stateKey].push_back(temp);
        }
    }
    // convert carrier frequency from GHz to Hz
    double f = Hz(leg.carrierFrequency).get();

    // get transmission time start (TTI = 1ms)
    simtime_t t = simTime().dbl() - 0.001;

    double re_h = 0;
    double im_h = 0;

    const JakesFadingData& actualJakesData = actualJakesMap.at(stateKey).at(band);

    // Compute Doppler shift.
    double doppler_shift = (speed * f) / SPEED_OF_LIGHT;

    for (int i = 0; i < numFadingPaths_; i++) {
        // Phase shift due to Doppler => t-selectivity.
        double phi_d = actualJakesData.angleOfArrival[i] * doppler_shift;

        // Phase shift due to delay spread => f-selectivity.
        double phi_i = actualJakesData.delaySpread[i].dbl() * f;

        // Calculate resulting phase due to t-selective and f-selective fading.
        double phi = 2.00 * M_PI * (phi_d * t.dbl() - phi_i);

        // One ring model/Clarke's model plus f-selectivity according to Cavers:
        // Due to isotropic antenna gain pattern on all paths only a^2 can be received on all paths.
        // Since we are interested in attenuation a := 1, attenuation per path is then:
        double attenuation = (1.00 / sqrt(static_cast<double>(numFadingPaths_)));

        // Convert to cartesian form and aggregate {Re, Im} over all fading paths.
        re_h = re_h + attenuation * cos(phi);
        im_h = im_h - attenuation * sin(phi);
    }

    // Output: |H_f|^2 = absolute channel impulse response due to fading.
    // Note that this may be >1 due to constructive interference.
    return linearToDb(re_h * re_h + im_h * im_h);
}

double RadioMedium::rayleighFading(StochasticChannelModel *radio, MacNodeId id, unsigned int band)
{
    // get rayleigh variable from trace file
    const int channelndex = 0;
    double temp1 = radio->getBinder()->phyPisaData.getChannel(channelndex + band);
    return linearToDb(temp1);
}

double RadioMedium::applyFading(StochasticChannelModel *radio, MacNodeId nodeId,
        const LinkKey& key, double speed, unsigned int band, bool isBgUe)
{
    double fadingAttenuation = 0;
    // if fading is enabled
    if (fading_) {
        // Applying fading
        if (fadingType_ == "RAYLEIGH")
            fadingAttenuation = rayleighFading(radio, nodeId, band);

        else if (fadingType_ == "JAKES")
            fadingAttenuation = jakesFading(radio, key, speed, band, isBgUe);
    }
    return fadingAttenuation;
}

double RadioMedium::computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord)
{
    double speed = 0.0;

    // createIfMissing=false: a node with no history yet must stay absent,
    // not gain an empty placeholder queue that a later front()/back() would
    // read as an entry
    std::queue<Position> *history = positionHistory(nodeId, legFor(radio), false);

    if (history == nullptr) {
        // no entries
        return speed;
    }
    else {
        //compute distance traveled from last update by UE (eNodeB position is fixed)

        if (history->size() == 1) {
            //  the only element refers to the present, return 0
            return speed;
        }

        double movement = history->front().second.distance(coord);

        if (movement <= 0.0)
            return speed;
        else {
            double time = (NOW.dbl()) - (history->front().first.dbl());
            if (time <= 0.0) // time not updated since last speed call
                throw cRuntimeError("Multiple entries detected in position history referring to the same time");
            // compute speed
            speed = (movement) / (time);
        }
    }
    return speed;
}

double RadioMedium::computeCorrelationDistance(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord)
{
    double dist = 0.0;

    bool existed = false;
    Position& point = correlationPoint(nodeId, legFor(radio), &existed);

    if (!existed) {
        // no lastCorrelationPoint found. Add current position and return dist = 0.0
        point = Position(NOW, coord);
    }
    else {
        dist = point.second.distance(coord);
    }
    return dist;
}

double RadioMedium::computeSectorAttenuation(StochasticChannelModel *radio, MacNodeId txId, GHz carrierFrequency,
        const inet::Coord& txCoord, const inet::Coord& rxCoord) const
{
    if (txDirectionOf(txId, carrierFrequency) != ANISOTROPIC)
        return 0.0; // antenna is omni-directional
    double txAngle = txAngleOf(txId, carrierFrequency);

    // compute the angle between the receiver position and the reference axis,
    // considering the transmitter as center
    double ueAngle = radio->computeAngle(txCoord, rxCoord);

    // compute the reception angle
    double recvAngle = fabs(txAngle - ueAngle);
    if (recvAngle > 180)
        recvAngle = 360 - recvAngle;

    double verticalAngle = radio->computeVerticalAngle(txCoord, rxCoord);

    // compute attenuation due to sectorial tx
    return radio->computeAngularAttenuation(recvAngle, verticalAngle);
}

std::vector<double> RadioMedium::getSINR(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo)
{
    RadioLink link = radio->linkFor(lteInfo);

    EV << "------------ GET SINR ----------------" << endl;

    // The desired signal: path loss, shadowing and fading. getSINR() below adds
    // noise and interference on top of it.
    return getSINR(radio, link, lteInfo, getRSRP(radio, link, lteInfo->getTxPower()));
}

std::vector<double> RadioMedium::getSINR(StochasticChannelModel *radio, const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector)
{
    // Get the Resource Blocks used to transmit this packet
    RbMap rbmap = lteInfo->getGrantedBlocks();

    /*
     * The SINR will be calculated as follows
     *
     *              Pwr
     * SINR = ---------
     *           N  +  I
     *
     * Ndb = thermalNoise + noiseFigure (measured in decibels)
     */


    // compute and linearize total noise
    double totN = dBmToLinear(thermalNoise_ + link.noiseFigure);

    // per-band interference-plus-noise denominator, in dBm
    std::vector<double> den(radio->getNumBands(radio->getCarrierFrequency()), 0.0);
    computeInterferencePlusNoise(radio, link, lteInfo, rbmap, totN, den);

    double sumSnr = 0.0;
    int usedRBs = 0;
    for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
        // if we are decoding a data transmission and this RB has not been used, skip it
        // TODO fix for multi-antenna case
        if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
            continue;

        // compute final SINR. Subtraction in dB is equivalent to linear division
        snrVector[i] -= den[i];

        sumSnr += snrVector[i];
        ++usedRBs;
    }

    MacNodeId ueId = link.txIsBaseStation ? link.rxId : link.txId;

    // emit SINR statistic. Only DL and UL have a measured-SINR signal; other link
    // types must not be reported as one of them.
    if (radio->getCollectSinrStatistics() && (lteInfo->getFrameType() == FEEDBACKPKT) && usedRBs > 0
        && (link.dir == DL || link.dir == UL))
    {
        // we are on the BS, so we need to retrieve the channel model of the sender
        // XXX I know, there might be a faster way...
        StochasticChannelModel *ueChannelModel = check_and_cast<StochasticChannelModel *>(
                check_and_cast<PhyUe *>(radio->getBinder()->getPhyByNodeId(ueId))->getChannelModel(lteInfo->getCarrierFrequency()));

        ueChannelModel->emitMeasuredSinr(link.dir, sumSnr / usedRBs);
    }

    // if sender is an eNodeB
    if (link.dir == DL)
        // store the position of user
        updatePositionHistory(radio, ueId, radio->getCoord());
    // sender is a UE
    else
        updatePositionHistory(radio, ueId, lteInfo->getCoord());
    return snrVector;
}

void RadioMedium::computeInterferencePlusNoise(StochasticChannelModel *radio, const RadioLink& link, UserControlInfo *lteInfo,
        RbMap& rbmap, double totN, std::vector<double>& den)
{
    if (link.dir == D2D || link.dir == D2D_MULTI) {
        const RadioDescriptor& d = descriptorFor(radio->getNodeId(), radio->getCarrierFrequency());
        if (d.d2dEndpoint == nullptr)
            throw cRuntimeError("computeInterferencePlusNoise: D2D/D2D_MULTI link direction on non-D2D-capable radio '%s'", radio->getFullPath().c_str());

        /*
         * In calculating a D2D CQI, the interference from other D2D UEs discriminates between calculating a CQI
         * in the direction D2D_Tx--->D2D_Rx or D2D_Tx<---D2D_Rx (This happens due to the different positions of the
         * interfering UEs relative to the position of the UE for whom we are calculating the CQI). We need that the CQI
         * for the D2D_Tx is the same as the D2D_Rx (This is a help for the simulator because when the eNodeB allocates
         * resources to a D2D_Tx it must refer to the quality channel of the D2D_Rx).
         * To do so, here we must check if the ueId is the ID of the D2D_Tx: if it
         * is so we swap the ueId with the one of his Peer (D2D_Rx). We do the same for the coord.
         */
        // vector containing the sum of inCell interference for each band
        std::vector<double> d2dInterference; // Linear value (mW)
        d2dInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
        if (d2dInterference_) {
            interference_->computeD2DInterference(radio, link.cellId, link.txId, link.txCoord, link.rxId, link.rxCoord,
                    (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), &d2dInterference, link.dir);
        }

        EV << "RadioMedium::computeInterferencePlusNoise - distance from my Peer = "
           << link.rxCoord.distance(link.txCoord) << " - DIR=" << dirToA(link.dir) << endl;

        // One loop for both cases: with interference disabled d2dInterference is all
        // zeros, so this degenerates to the noise-only denominator. Because this goes
        // through dBm -> linear -> dBm rather than summing noise directly in dBm, the
        // two are not bit-identical -- observable only when D2D interference is
        // disabled, which no shipped configuration does.
        for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
            // the caller skips these bands too; leave their denominator untouched
            if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
                continue;

            den[i] = linearToDBm(totN + d2dInterference[i]);

            EV << "\t in[" << d2dInterference[i] << "] - den[" << den[i] << "]\n";
        }
        return;
    }

    // The interference model is cellular-topology-aware (it asks "which cell?"),
    // so recover the UE/BS roles from the link. The propagation math does not need them.
    Direction dir = link.dir;
    MacNodeId ueId = link.txIsBaseStation ? link.rxId : link.txId;
    MacNodeId eNbId = link.cellId;
    inet::Coord ueCoord = link.txIsBaseStation ? link.rxCoord : link.txCoord;
    inet::Coord enbCoord = link.txIsBaseStation ? link.txCoord : link.rxCoord;


    //============ MULTI CELL INTERFERENCE COMPUTATION =================
    // vector containing the sum of multi-cell interference for each band
    std::vector<double> multiCellInterference; // Linear value (mW)
    // prepare data structure
    multiCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (downlinkInterference_ && dir == DL && lteInfo->getFrameType() != BEACONPKT) {
        interference_->computeDownlinkInterference(radio, eNbId, ueId, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }
    else if (uplinkInterference_ && dir == UL) {
        interference_->computeUplinkInterference(radio, eNbId, ueId, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }

    //============ BACKGROUND CELLS INTERFERENCE COMPUTATION =================
    // vector containing the sum of background cell interference for each band
    std::vector<double> bgCellInterference; // Linear value (mW)
    // prepare data structure
    bgCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (bgCellInterference_) {
        interference_->computeBackgroundCellInterference(radio, ueId, enbCoord, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, dir, &bgCellInterference); // dBm
    }

    //============ EXTCELL INTERFERENCE COMPUTATION =================
    // TODO this might be obsolete as it is replaced by background cell interference
    // vector containing the sum of external cell interference for each band
    std::vector<double> extCellInterference; // Linear value (mW)
    // prepare data structure
    extCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (extCellInterference_ && dir == DL) {
        interference_->computeExtCellInterference(radio, eNbId, ueId, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), &extCellInterference); // dBm
    }

    EV << "RadioMedium::computeInterferencePlusNoise - distance from my eNb=" << enbCoord.distance(ueCoord) << " - DIR=" << ((dir == DL) ? "DL" : "UL") << endl;

    for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
        // the caller skips these bands too; leave their denominator untouched
        if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
            continue;

        //                  (      mW              +          mW            +  mW  +        mW            )
        den[i] = linearToDBm(bgCellInterference[i] + extCellInterference[i] + totN + multiCellInterference[i]);

        EV << "\t bgCell[" << bgCellInterference[i] << "] - ext[" << extCellInterference[i] << "] - multi[" << multiCellInterference[i]
           << "] - den[" << den[i] << "]\n";
    }
}

std::vector<double> RadioMedium::getRSRP(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return getRSRP(radio, radio->linkFor(lteInfo), lteInfo->getTxPower());
}

std::vector<double> RadioMedium::getRSRP(StochasticChannelModel *radio, const RadioLink& link, double txPower)
{
    double recvPower = txPower; // dBm

    EV << "RadioMedium::getRSRP - txId=" << link.txId
       << " - rxId=" << link.rxId
       << " - DIR=" << dirToA(link.dir)
       << " - txPwr " << txPower
       << " - txCoord[" << link.txCoord << "] - rxCoord[" << link.rxCoord << "]" << endl;

    // =============== PATH LOSS + SHADOWING + FADING =================
    EV << "\t using parameters - noiseFigure=" << link.noiseFigure
       << " - antennaGainTx=" << link.txAntennaGain << " - antennaGainRx=" << link.rxAntennaGain
       << " - txPwr=" << txPower << " - for nodeId=" << link.stateKey << endl;

    // Speed must be read BEFORE getAttenuation(), which appends to the position
    // history: computeSpeed() derives from that history, so evaluating it
    // afterwards would yield a different value and hence different fading.
    // Load-bearing ordering.
    double speed = computeSpeed(radio, link.stateNodeId, link.stateCoord);

    // attenuation for the desired signal. getAttenuation() below recomputes
    // computeSpeed() a second time on the same (radio, stateNodeId, stateCoord)
    // -- idempotent, since it runs before its own position-history append too,
    // so both calls read the same history and agree; reader-burden only.
    double attenuation = getAttenuation(radio, link); // dB

    // compute attenuation (PATHLOSS + SHADOWING)
    recvPower -= attenuation; // (dBm-dB)=dBm

    // add antenna gain
    recvPower += link.txAntennaGain; // (dBm+dB)=dBm
    recvPower += link.rxAntennaGain; // (dBm+dB)=dBm

    // sub cable loss
    recvPower -= radio->getCableLoss(); // (dBm-dB)=dBm

    // =============== ANGULAR ATTENUATION =================
    // Only a base station has a sectorial antenna; a UE-to-UE link never gets here.
    if (link.txIsBaseStation)
        recvPower -= computeSectorAttenuation(radio, link.txId, radio->getCarrierFrequency(), link.txCoord, link.rxCoord);
    // =============== END ANGULAR ATTENUATION =================

    std::vector<double> rsrpVector;
    rsrpVector.resize(radio->getNumBands(radio->getCarrierFrequency()), 0.0);

    // compute and add interference due to fading
    // Apply fading for each band
    // if the phy layer is localized we can assume that for each logical band we have different fading attenuation
    // if the phy layer is distributed the number of logical bands should be set to 1
    double fadingAttenuation = 0;


    // for each logical band
    // FIXME compute fading only for used RBs
    for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
        fadingAttenuation = applyFading(radio, link.stateNodeId, link.stateKey, speed, i);

        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        EV << " RadioMedium::getRSRP node " << link.stateKey
           << " band " << i << " recvPower " << recvPower
           << " direction " << dirToA(link.dir) << " antenna gain tx "
           << link.txAntennaGain << " antenna gain rx " << link.rxAntennaGain
           << " noise figure " << link.noiseFigure
           << " cable loss   " << radio->getCableLoss()
           << " attenuation (pathloss + shadowing) " << attenuation
           << " speed " << speed << " thermal noise " << thermalNoise_
           << " fading attenuation " << fadingAttenuation << endl;

        rsrpVector[i] = finalRecvPower;
    }
    // ============ END PATH LOSS + SHADOWING + FADING ===============

    return rsrpVector;
}

std::vector<double> RadioMedium::getSINR_bgUe(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo)
{
    //get tx power
    double recvPower = lteInfo->getTxPower(); // dBm

    // get MacId and Direction
    MacNodeId bgUeId = lteInfo->getSourceId();
    MacNodeId eNbId = lteInfo->getDestId();
    Direction dir = lteInfo->getDirection();

    // position of e/gNb and UE
    inet::Coord ueCoord = lteInfo->getCoord();
    inet::Coord enbCoord = radio->getCoord();

    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;
    double noiseFigure = 0.0;
    double speed = 0.0;

    EV << "------------ GET SINR for background UE ----------------" << endl;
    //===================== PARAMETERS SETUP ============================
    /*
     * This function is called on the e/gNodeB side and is similar
     * to what is called when computing feedback
     */
    if (dir == DL) {
        //set noise figure
        noiseFigure = radio->getUeNoiseFigure(); //dB
        //set antenna gain figure
        antennaGainTx = radio->getAntennaGainEnB(); //dB
        antennaGainRx = radio->getAntennaGainUe();  //dB
    }
    else { // if( dir == UL )
        // TODO check if antennaGainEnB should be added in UL direction too
        antennaGainTx = radio->getAntennaGainUe();
        antennaGainRx = radio->getAntennaGainEnB();
        noiseFigure = radio->getBsNoiseFigure();
    }
    speed = computeSpeed(radio, bgUeId, ueCoord);

    CellInfo *eNbCell = radio->getBinder()->getCellInfoByNodeId(eNbId);
    const char *eNbTypeString = eNbCell ? (eNbCell->getEnbType() == MACRO_ENB ? "MACRO" : "MICRO") : "NULL";

    EV << "RadioMedium::getSINR_bgUe - DIR=" << ((dir == DL) ? "DL" : "UL")
       << " " << eNbTypeString << " - txPwr " << lteInfo->getTxPower()
       << " - ueCoord[" << ueCoord << "] - enbCoord[" << enbCoord << "] - enbId[" << eNbId << "]" <<
        endl;

    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS + ANGULAR ATTENUATION + FADING =================
    // getAttenuation() applies shadowing only for a real UE (BGUE_MIN_ID-gated),
    // so for this background UE it reduces to path loss; fading is applied per
    // band below.

    // UL because we are computing a feedback
    double attenuation = radio->getAttenuation(bgUeId, UL, ueCoord);

    //compute recvPower
    recvPower -= attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm
    //sub cable loss
    recvPower -= radio->getCableLoss(); // (dBm-dB)=dBm

    // ANGULAR ATTENUATION
    if (dir == DL)
        recvPower -= computeSectorAttenuation(radio, eNbId, radio->getCarrierFrequency(), enbCoord, ueCoord);

    std::vector<double> snrVector;
    snrVector.resize(radio->getNumBands(radio->getCarrierFrequency()), recvPower);


    // for each logical band
    double fadingAttenuation = 0;
    for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
        fadingAttenuation = applyFading(radio, bgUeId, LinkKey(bgUeId), speed, i, true);
        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        snrVector[i] = finalRecvPower;
    }

    //============ END PATH LOSS + ANGULAR ATTENUATION + FADING ===============

    /*
     * The SINR will be calculated as follows
     *
     *           Pwr
     * SINR = ---------
     *         N  +  I
     *
     * Ndb = thermalNoise + noiseFigure (measured in decibel)
     * I = extCellInterference + multiCellInterference
     */

    // TODO Interference computation still needs to be implemented

    //============ MULTI CELL INTERFERENCE COMPUTATION =================
    // for background UEs, we only compute CQI
    bool isCqi = true;
    RbMap rbmap;
    //vector containing the sum of multicell interference for each band
    std::vector<double> multiCellInterference; // Linear value (mW)
    // prepare data structure
    multiCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (downlinkInterference_ && dir == DL) {
        interference_->computeDownlinkInterference(radio, eNbId, bgUeId, ueCoord, isCqi, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }
    else if (uplinkInterference_ && dir == UL) {
        interference_->computeUplinkInterference(radio, eNbId, bgUeId, isCqi, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }

    //============ BACKGROUND CELLS INTERFERENCE COMPUTATION =================
    //vector containing the sum of bg-cell interference for each band
    std::vector<double> bgCellInterference; // Linear value (mW)
    // prepare data structure
    bgCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (bgCellInterference_) {
        interference_->computeBackgroundCellInterference(radio, bgUeId, enbCoord, ueCoord, isCqi, lteInfo->getCarrierFrequency(), rbmap, dir, &bgCellInterference); // dBm
    }

    //============ EXTCELL INTERFERENCE COMPUTATION =================
    // TODO this might be obsolete as it is replaced by background cell interference
    //vector containing the sum of ext-cell interference for each band
    std::vector<double> extCellInterference; // Linear value (mW)
    // prepare data structure
    extCellInterference.resize(radio->getNumBands(radio->getCarrierFrequency()), 0);
    if (extCellInterference_ && dir == DL) {
        interference_->computeExtCellInterference(radio, eNbId, bgUeId, ueCoord, isCqi, lteInfo->getCarrierFrequency(), &extCellInterference); // dBm
    }

    //===================== SINR COMPUTATION ========================
    // compute and linearize total noise
    double totN = dBmToLinear(thermalNoise_ + noiseFigure);

    // add interference for each band
    for (unsigned int i = 0; i < radio->getNumBands(radio->getCarrierFrequency()); i++) {
        // denominator expressed in dBm as (N+extCell+multiCell)
        //               (      mW              +          mW            +  mW  +        mW            )
        double den = linearToDBm(bgCellInterference[i] + extCellInterference[i] + totN + multiCellInterference[i]);

        EV << "\t bgCell[" << bgCellInterference[i] << "] - ext[" << extCellInterference[i] << "] - multi[" << multiCellInterference[i] << "] - recvPwr["
           << dBmToLinear(snrVector[i]) << "] - sinr[" << snrVector[i] - den << "]\n";

        // compute final SINR
        snrVector[i] -= den;
    }

    return snrVector;
}

double RadioMedium::getReceivedPower_bgUe(StochasticChannelModel *radio, double txPower, inet::Coord txPos, inet::Coord rxPos,
        Direction dir, bool losStatus, MacNodeId bsId)
{
    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;

    EV << NOW << " RadioMedium::getReceivedPower_bgUe" << endl;

    //===================== PARAMETERS SETUP ============================
    if (dir == DL) {
        antennaGainTx = radio->getAntennaGainEnB(); //dB
        antennaGainRx = radio->getAntennaGainUe();  //dB
    }
    else { // if( dir == UL )
        antennaGainTx = radio->getAntennaGainUe();
        antennaGainRx = radio->getAntennaGainEnB();
    }

    EV << "RadioMedium::getReceivedPower_bgUe - DIR=" << ((dir == DL) ? "DL" : "UL")
       << " - txPwr " << txPower << " - txPos[" << txPos << "] - rxPos[" << rxPos << "] " << endl;
    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS + ANGULAR ATTENUATION =================
    // A single scalar received-power estimate, computed via computePathLoss()
    // directly (not getAttenuation()): no shadowing, and no per-band fading
    // either.

    //compute attenuation based on selected scenario and based on LOS or NLOS
    double sqrDistance = txPos.distance(rxPos);
    double dbp = 0;
    // No specific bg-UE identity reaches this call (§3(k)): radio is always
    // the eNB side, so linkContextFor() falls back to the UE-side default height.
    double attenuation = computePathLoss(radio, sqrDistance, dbp, losStatus, NODEID_NONE);

    //compute recvPower
    double recvPower = txPower - attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm
    //sub cable loss
    recvPower -= radio->getCableLoss(); // (dBm-dB)=dBm

    // ANGULAR ATTENUATION
    if (dir == DL)
        recvPower -= computeSectorAttenuation(radio, bsId, radio->getCarrierFrequency(), txPos, rxPos);
    //============ END PATH LOSS + ANGULAR ATTENUATION ===============

    return recvPower;
}

bool RadioMedium::isReceptionSuccessful(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
        const std::vector<double>& rsrpVector)
{
    EV << "RadioMedium::isReceptionSuccessful" << endl;

    // get codeword
    unsigned char cw = lteInfo->getCw();
    // get number of codewords
    int size = lteInfo->getUserTxParams()->readCqiVector().size();

    // if total number of codewords is equal to 1 the cw index should be only 0
    if (size == 1)
        cw = 0;

    // get cqi used to transmit this cw
    Cqi cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];

    MacNodeId id;
    Direction dir = lteInfo->getDirection();

    // Get MacNodeId of UE
    if (dir == DL)
        id = lteInfo->getDestId();
    else
        id = lteInfo->getSourceId();

    // Get Number of transmission attempts (includes original + retransmissions)
    unsigned char transmissionAttempt = lteInfo->getTxNumber();

    // consistency check
    if (transmissionAttempt == 0)
        throw cRuntimeError("Transmissions counter should not be 0");

    // Get txmode
    TxMode txmode = (TxMode)lteInfo->getTxMode();

    // Take sinr (getReceptionSinr() routes D2D/D2D_MULTI receptions through
    // getSINR_D2D on radio's own D2D marker)
    std::vector<double> snrV = getReceptionSinr(radio, frame, lteInfo, rsrpVector);

    // Get the resource Block id used to transmit this packet
    RbMap rbmap = lteInfo->getGrantedBlocks();

    // Get txmode
    unsigned int itxmode = txModeToIndex[txmode];

    double blockErrorRate = 0.0;
    double cumulativeSuccessProbability = 1.0;

    // for statistical purposes
    double sumSnr = 0.0;
    int usedRBs = 0;

    // for each Remote unit used to transmit the packet
    for (const auto &[remoteUnit, rbList] : rbmap) {
        // for each logical band used to transmit the packet
        for (const auto &[band, allocation] : rbList) {
            // this Rb is not allocated
            if (allocation == 0)
                continue;

            // Get the Bler
            if (cqi == 0)
                return false; // CQI 0 means channel below usable quality (e.g. after handover) -- loss
            if (cqi > 15)
                throw cRuntimeError("A packet has been transmitted with a cqi greater than 15 cqi:%d txmode:%d dir:%d rb:%d cw:%d rtx:%d", cqi, lteInfo->getTxMode(), dir, band, cw, transmissionAttempt);

            // for statistical purposes
            sumSnr += snrV[band];
            usedRBs++;

            int snr = snrV[band];// XXX because band is a Band (=unsigned short)
            if (snr < radio->getBinder()->phyPisaData.minSnr())
                return false;
            else if (snr > radio->getBinder()->phyPisaData.maxSnr())
                blockErrorRate = 0.0;
            else
                blockErrorRate = radio->getBinder()->phyPisaData.getBler(itxmode, cqi, snr);

            EV << "\t bler computation: [itxMode=" << itxmode << "] - [cqi=" << cqi
               << "] - [snr=" << snr << "]" << endl;

            double blockSuccessRate = 1.0 - blockErrorRate;
            // compute the success probability according to the number of RB used
            double allocationSuccessProbability = pow(blockSuccessRate, (double)allocation);
            // compute the success probability according to the number of LB used
            cumulativeSuccessProbability *= allocationSuccessProbability;

            EV << " RadioMedium::isReceptionSuccessful direction " << dirToA(dir)
               << " node " << id << " remote unit " << dasToA(remoteUnit)
               << " Band " << band << " SNR " << snr << " CQI " << cqi
               << " BLER " << blockErrorRate << " success probability " << allocationSuccessProbability
               << " total success probability " << cumulativeSuccessProbability << endl;
        }
    }
    // Compute total error probability
    double packetErrorRate = 1.0 - cumulativeSuccessProbability;
    // Apply HARQ soft combining gain
    double effectiveErrorRateWithHarq = packetErrorRate * pow(harqReduction_, transmissionAttempt - 1);

    double randomSample = uniform(0.0, 1.0);

    EV << " RadioMedium::isReceptionSuccessful direction " << dirToA(dir)
       << " node " << id << " total ERROR probability  " << packetErrorRate
       << " per with H-ARQ error reduction " << effectiveErrorRateWithHarq
       << " - CQI[" << cqi << "]- random error extracted[" << randomSample << "]" << endl;

    // emit SINR statistic
    if (radio->getCollectSinrStatistics() && usedRBs > 0)
        emitRcvdSinr(radio, dir, id, lteInfo->getCarrierFrequency(), sumSnr / usedRBs);

    bool receptionFailed = (randomSample <= effectiveErrorRateWithHarq);
    if (receptionFailed) {
        EV << "This is NOT your lucky day (" << randomSample << " < " << effectiveErrorRateWithHarq
           << ") -> do not receive." << endl;

        // Signal too weak, we can't receive it
        return false;
    }
    // Signal is strong enough, receive this Signal
    EV << "This is your lucky day (" << randomSample << " > " << effectiveErrorRateWithHarq
       << ") -> Receive AirFrame." << endl;

    return true;
}

RadioLink RadioMedium::d2dLink(StochasticChannelModel *radio, MacNodeId srcId, inet::Coord srcCoord,
        MacNodeId destId, inet::Coord destCoord)
{
    RadioLink link;
    link.dir = D2D;

    link.txId = srcId;
    link.rxId = destId;
    link.txCoord = srcCoord;
    link.rxCoord = destCoord;

    // Both endpoints are UEs.
    link.txAntennaGain = link.rxAntennaGain = radio->getAntennaGainUe();
    link.noiseFigure = radio->getUeNoiseFigure();
    link.txIsBaseStation = false; // omnidirectional: no angular attenuation

    // radio's own serving cell -- for height role-resolution (E4/§3(k), risk
    // 16): both D2D ends are UEs, so h_BS is resolved via the serving cell,
    // not via either end.
    link.cellId = radio->getBinder()->getServingNodeOrSelf(radio->getNodeId());

    // The channel state is keyed on the *link*, so a UE's several D2D peers each
    // get their own LOS state, shadowing realization and fading process, instead
    // of sharing the transmitter's single slot (and colliding with the
    // transmitter's own cellular state).
    //
    // The tracked node stays the transmitter: it is that UE's motion that
    // defines the speed, regardless of which end (radio) is asking.
    link.stateKey = LinkKey(srcId, destId);
    link.stateNodeId = srcId;
    link.stateCoord = srcCoord;

    return link;
}

std::vector<double> RadioMedium::getReceptionSinr(StochasticChannelModel *radio, LteAirFrame *frame, UserControlInfo *lteInfo,
        const std::vector<double>& rsrpVector)
{
    Direction dir = lteInfo->getDirection();
    if (dir == D2D || dir == D2D_MULTI) {
        const RadioDescriptor& d = descriptorFor(radio->getNodeId(), radio->getCarrierFrequency());
        if (d.d2dEndpoint == nullptr)
            throw cRuntimeError("getReceptionSinr: D2D/D2D_MULTI direction on non-D2D-capable radio '%s'", radio->getFullPath().c_str());

        MacNodeId destId = lteInfo->getDestId();
        inet::Coord destCoord = radio->getCoord();
        MacNodeId enbId = radio->getBinder()->getServingNodeOrSelf(lteInfo->getSourceId());

        // One-to-many reception decides on the RSRP captured by the capture-effect
        // logic (see D2dUePhyHelper::storeAirFrame), so the desired signal is not
        // recomputed here.
        if (dir == D2D_MULTI)
            return d.d2dEndpoint->getSINR_D2D(frame, lteInfo, destId, destCoord, enbId, rsrpVector);
        return d.d2dEndpoint->getSINR_D2D(frame, lteInfo, destId, destCoord, enbId);
    }
    return getSINR(radio, frame, lteInfo);
}

void RadioMedium::emitRcvdSinr(StochasticChannelModel *radio, Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr)
{
    if (dir == D2D || dir == D2D_MULTI) {
        // A D2D reception is not an uplink reception. Attribute it to the receiver
        // -- radio, the same registered endpoint the pre-fold D2dChannelModel's own
        // emitRcvdSinr() emitted on as "this" -- rather than to the sender's channel
        // model the way the cellular UL branch below does.
        const RadioDescriptor& d = descriptorFor(radio->getNodeId(), radio->getCarrierFrequency());
        if (d.d2dEndpoint == nullptr)
            throw cRuntimeError("emitRcvdSinr: D2D/D2D_MULTI direction on non-D2D-capable radio '%s'", radio->getFullPath().c_str());
        radio->emit(d.d2dEndpoint->getRcvdSinrD2DSignal(), sinr);
        return;
    }

    if (dir == DL) { // we are on the UE
        radio->emitRcvdSinr(dir, sinr);
        return;
    }

    // we are on the BS, so we need to retrieve the channel model of the sender
    // XXX I know, there might be a faster way...
    StochasticChannelModel *ueChannelModel = check_and_cast<StochasticChannelModel *>(
            check_and_cast<PhyUe *>(radio->getBinder()->getPhyByNodeId(ueId))->getChannelModel(carrierFrequency));
    ueChannelModel->emitRcvdSinr(dir, sinr);
}

} //namespace
