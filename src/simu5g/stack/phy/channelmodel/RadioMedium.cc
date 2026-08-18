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

#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"
#include "simu5g/stack/phy/channelmodel/Tr36814PathLossModel.h"
#include "simu5g/stack/phy/channelmodel/Tr36873PathLossModel.h"
#include "simu5g/stack/phy/channelmodel/Tr38901PathLossModel.h"

namespace simu5g {

Define_Module(RadioMedium);

namespace {

/** The carrier leg a registered endpoint belongs to (plan 3(h)): its own carrier frequency plus its isNr flag. */
CarrierLeg legFor(StochasticChannelModel *endpoint)
{
    return CarrierLeg{endpoint->getCarrierFrequency(), endpoint->isNr()};
}

/*
 * The six per-radio stochastic-state accessors relocated alongside the
 * computation that draws through them (plan step S9b). They reproduce
 * StochasticChannelModel's own private accessors exactly (same map
 * operations, same semantics), parameterized over a PerRadioStochasticState
 * reference instead of an implicit state_ member, since the medium already
 * owns every registered radio's state (S8) and the computation that reads
 * it now runs here instead of on the endpoint.
 */

/** Auto-vivifying access to state.losMap[key]; existed, if given, reports whether the entry was already present. */
bool& losState(PerRadioStochasticState& state, const LinkKey& key, bool *existed = nullptr)
{
    auto result = state.losMap.try_emplace(key, false);
    if (existed != nullptr)
        *existed = !result.second;
    return result.first->second;
}

/** radio's shadowing-state container (whole-container seam: computeShadowing may redirect to a different radio's via obtainShadowingMap()). */
ShadowFadingMap& shadowingState(PerRadioStochasticState& state) { return state.lastComputedSF; }

/** radio's (non-background-UE) Jakes-fading-state container. */
JakesFadingMap& jakesState(PerRadioStochasticState& state) { return state.jakesFadingMap; }

/** jakesFadingMap's background-UE twin: same key type, selected instead of it when isBgUe is true. */
JakesFadingMap& jakesStateBgUe(PerRadioStochasticState& state) { return state.jakesFadingMapBgUe; }

/**
 * Access to state.positionHistory[nodeId]. createIfMissing=true
 * auto-vivifies an empty queue like std::map::operator[]; createIfMissing=false
 * returns nullptr instead of inserting a placeholder for a node with no
 * history yet (computeSpeed(), which must not manufacture an entry it would
 * then read as non-empty).
 */
std::queue<Position> *positionHistory(PerRadioStochasticState& state, MacNodeId nodeId, bool createIfMissing)
{
    if (createIfMissing)
        return &state.positionHistory[nodeId];
    auto it = state.positionHistory.find(nodeId);
    return it == state.positionHistory.end() ? nullptr : &it->second;
}

/** Auto-vivifying access to state.lastCorrelationPoint[key]; existed, if given, reports whether the entry was already present. */
Position& correlationPoint(PerRadioStochasticState& state, const LinkKey& key, bool *existed = nullptr)
{
    auto result = state.lastCorrelationPoint.try_emplace(key);
    if (existed != nullptr)
        *existed = !result.second;
    return result.first->second;
}

} // namespace

RadioMedium::~RadioMedium()
{
    for (auto& [leg, model] : pathLoss_)
        delete model;
}

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
    CarrierLeg leg = legFor(endpoint);
    CarrierPhysics candidate = readCarrierPhysics(endpoint);
    auto cpIt = carrierPhysics_.find(leg);
    if (cpIt == carrierPhysics_.end()) {
        candidate.establishedByPath = endpoint->getFullPath();
        carrierPhysics_[leg] = candidate;

        // this leg's path-loss strategy (S9b), built from the record just
        // established -- never from this radio's own members (plan 3(i).4)
        pathLoss_[leg] = createPathLossModel(candidate, leg);
    }
    else {
        checkCarrierPhysics(cpIt->second, candidate, leg, endpoint->getFullPath());
    }

    // create this radio's stochastic-state record (S8); stateOf() hands the
    // endpoint a reference to it right after this call
    radioState_[endpoint];

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

    radioState_.erase(endpoint);
}

PerRadioStochasticState& RadioMedium::stateOf(StochasticChannelModel *endpoint)
{
    auto it = radioState_.find(endpoint);
    if (it == radioState_.end())
        throw cRuntimeError("stateOf: endpoint was never registered with this medium");
    return it->second;
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

O2iState RadioMedium::o2iStateOf(MacNodeId nodeId, GHz carrierFrequency) const
{
    const RadioDescriptor& d = descriptorFor(nodeId, carrierFrequency);
    return O2iState{d.endpoint->getInsideBuilding(), d.endpoint->getInsideDistance()};
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

const CarrierPhysics& RadioMedium::carrierPhysicsFor(const CarrierLeg& leg) const
{
    auto it = carrierPhysics_.find(leg);
    if (it == carrierPhysics_.end())
        throw cRuntimeError("no carrier physics record for carrier leg %gGHz/%s",
                leg.carrierFrequency.get(), leg.isNr ? "NR" : "LTE");
    return it->second;
}

PathLossModel *RadioMedium::createPathLossModel(const CarrierPhysics& cp, const CarrierLeg& leg)
{
    PathLossModel *model;
    if (cp.pathLossType == "Tr36814")
        model = new Tr36814PathLossModel();
    else if (cp.pathLossType == "Tr36873")
        model = new Tr36873PathLossModel();
    else if (cp.pathLossType == "Tr38901") {
        auto *tr38901 = new Tr38901PathLossModel();
        tr38901->setUseBuildingPenetrationHighLossModel(cp.useBuildingPenetrationHighLossModel);
        model = tr38901;
    }
    else
        throw cRuntimeError("Unrecognized value in 'pathLossType' parameter: \"%s\"", cp.pathLossType.c_str());

    // the frequency triple reproduces ChannelModelBase's own derivation
    // (ChannelModelBase.cc:26-29) from the leg's own carrier frequency, with
    // no round-trip through an endpoint (plan 3(i).4)
    double carrierFrequencyGHz = GHz(leg.carrierFrequency).get();
    double carrierFrequencyHz = Hz(leg.carrierFrequency).get();
    model->initialize(this, aToDeploymentScenario(cp.scenario), cp.nodebHeight, cp.ueHeight, cp.buildingHeight, cp.streetWidth,
            carrierFrequencyHz, carrierFrequencyGHz, log10(carrierFrequencyGHz),
            cp.tolerateMaxDistViolation);
    return model;
}

double RadioMedium::getAttenuation(StochasticChannelModel *radio, const RadioLink& link)
{
    double threeDimDistance = link.txCoord.distance(link.rxCoord);
    double twoDimDistance = radio->getTwoDimDistance(link.txCoord, link.rxCoord);

    double speed = computeSpeed(radio, link.stateNodeId, link.stateCoord);
    double correlationDist = computeCorrelationDistance(radio, link.stateKey, link.stateCoord);

    PerRadioStochasticState& state = stateOf(radio);
    const CarrierPhysics& cp = carrierPhysicsFor(legFor(radio));

    // If Euclidean distance since last LOS probability computation is greater than
    // correlation distance the UE could have changed its state and
    // its visibility from eNodeB, hence it is correct to recompute the LOS probability
    bool losAlreadyComputed = false;
    losState(state, link.stateKey, &losAlreadyComputed);
    if (correlationDist > cp.correlationDistance || !losAlreadyComputed) {
        computeLosProbability(radio, threeDimDistance, twoDimDistance, link.stateKey);
    }

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losState(state, link.stateKey);
    double attenuation = computePathLoss(radio, threeDimDistance, twoDimDistance, los);

    //    Applying shadowing only if it is enabled by configuration
    //    log-normal shadowing (not available for background UEs)
    if (num(link.stateNodeId) < BGUE_MIN_ID && cp.shadowing)
        attenuation += computeShadowing(radio, threeDimDistance, twoDimDistance, link.stateKey, link.stateNodeId, speed, link.useUeSideMaps);

    // update the tracked node's current position
    radio->updatePositionHistory(link.stateNodeId, link.stateCoord);
    radio->updateCorrelationDistance(link.stateKey, link.stateCoord);

    EV << "RadioMedium::getAttenuation - computed attenuation at distance " << threeDimDistance << " for eNB is " << attenuation << endl;

    return attenuation;
}

double RadioMedium::computePathLoss(StochasticChannelModel *radio, double distance, double dbp, bool los)
{
    O2iState o2i = o2iStateOf(radio->getNodeId(), radio->getCarrierFrequency());
    return pathLossFor(legFor(radio)).computePathLoss(distance, dbp, los, o2i);
}

void RadioMedium::computeLosProbability(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key)
{
    const CarrierLeg leg = legFor(radio);
    const CarrierPhysics& cp = carrierPhysicsFor(leg);
    PerRadioStochasticState& state = stateOf(radio);

    if (!cp.dynamicLos) {
        losState(state, key) = cp.fixedLos;
        return;
    }
    double p = pathLossFor(leg).computeLosProbability(d3D, d2D);
    losState(state, key) = (uniform(0.0, 1.0) <= p);
}

double RadioMedium::computeShadowing(StochasticChannelModel *radio, double d3D, double d2D, const LinkKey& key,
        MacNodeId ownerId, double speed, bool cqiDl)
{
    const CarrierLeg leg = legFor(radio);
    PerRadioStochasticState& state = stateOf(radio);
    const CarrierPhysics& cp = carrierPhysicsFor(leg);

    ShadowFadingMap *actualShadowingMap;
    if (cqiDl) // if we are computing a DL CQI we need the Shadowing Map stored on the UE side
        actualShadowingMap = radio->obtainShadowingMap(ownerId);
    else
        actualShadowingMap = &shadowingState(state);

    if (actualShadowingMap == nullptr)
        throw cRuntimeError("RadioMedium::computeShadowing - actualShadowingMap not found (nullptr)");

    double mean = 0;

    // Get std deviation according to LOS/NLOS and selected scenario
    double stdDev = pathLossFor(leg).getShadowingStdDev(d3D, d2D, losState(state, key));
    double time = 0;
    double space = 0;
    double att;

    // if shadowing for current user has never been computed
    if (actualShadowingMap->find(key) == actualShadowingMap->end()) {
        //Get the log-normal shadowing with std deviation stdDev
        att = normal(mean, stdDev);

        //store the shadowing attenuation for this user and the temporal mark
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[key] = tmp;

        //If the shadowing attenuation has been computed at least one time for this user
        // and the distance traveled by the UE is greater than correlation distance
    }
    else if ((NOW - actualShadowingMap->at(key).first).dbl() * speed > cp.correlationDistance) {

        //get the temporal mark of the last computed shadowing attenuation
        time = (NOW - actualShadowingMap->at(key).first).dbl();

        //compute the traveled distance
        space = time * speed;

        //Compute shadowing with an EAW (Exponential Average Window) (step 1)
        double a = exp(-0.5 * (space / cp.correlationDistance));

        //Get last shadowing attenuation computed
        double old = actualShadowingMap->at(key).second;

        //Compute shadowing with an EAW (Exponential Average Window) (step 2)
        att = a * old + sqrt(1 - pow(a, 2)) * normal(mean, stdDev);

        // Store the new computed shadowing
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[key] = tmp;

        // if the distance traveled by the UE is smaller than correlation distance shadowing attenuation remains the same
    }
    else {
        att = actualShadowingMap->at(key).second;
    }

    return att;
}

double RadioMedium::jakesFading(StochasticChannelModel *radio, const LinkKey& key, MacNodeId ownerId, double speed,
        unsigned int band, bool cqiDl, bool isBgUe)
{
    /**
     * NOTE: there are two different Jakes maps. One on the UE side and one on the eNB side, with different values.
     *
     * eNB side => used for CQI computation and for error-probability evaluation in UL
     * UE side  => used for error-probability evaluation in DL
     *
     * the one within eNB is referred to the UL direction
     * the one within UE is referred to the DL direction
     *
     * thus the actual map should be chosen carefully (i.e. just check the cqiDL flag)
     */
    PerRadioStochasticState& state = stateOf(radio);
    JakesFadingMap *actualJakesMap;

    if (cqiDl) // if we are computing a DL CQI we need the Jakes Map stored on the UE side
        actualJakesMap = (!isBgUe) ? radio->obtainUeJakesMap(ownerId) : &jakesStateBgUe(state);
    else
        actualJakesMap = &jakesState(state);

    const CarrierLeg leg = legFor(radio);
    const CarrierPhysics& cp = carrierPhysicsFor(leg);

    // if this is the first time that we compute fading for current user
    if (actualJakesMap->find(key) == actualJakesMap->end()) {
        // clear the map
        // FIXME: possible memory leak
        (*actualJakesMap)[key].clear();

        // for each band we are going to create a Jakes fading
        for (unsigned int j = 0; j < radio->getNumBands(); j++) {
            // clear some structure
            JakesFadingData temp;
            temp.angleOfArrival.clear();
            temp.delaySpread.clear();

            // for each fading path
            for (int i = 0; i < cp.numFadingPaths; i++) {
                // get angle of arrivals
                temp.angleOfArrival.push_back(cos(uniform(0, M_PI)));

                // get delay spread
                temp.delaySpread.push_back(exponential(cp.delayRms));
            }
            // store the Jakes fading for this user
            (*actualJakesMap)[key].push_back(temp);
        }
    }
    // convert carrier frequency from GHz to Hz
    double f = Hz(leg.carrierFrequency).get();

    // get transmission time start (TTI = 1ms)
    simtime_t t = simTime().dbl() - 0.001;

    double re_h = 0;
    double im_h = 0;

    const JakesFadingData& actualJakesData = actualJakesMap->at(key).at(band);

    // Compute Doppler shift.
    double doppler_shift = (speed * f) / SPEED_OF_LIGHT;

    for (int i = 0; i < cp.numFadingPaths; i++) {
        // Phase shift due to Doppler => t-selectivity.
        double phi_d = actualJakesData.angleOfArrival[i] * doppler_shift;

        // Phase shift due to delay spread => f-selectivity.
        double phi_i = actualJakesData.delaySpread[i].dbl() * f;

        // Calculate resulting phase due to t-selective and f-selective fading.
        double phi = 2.00 * M_PI * (phi_d * t.dbl() - phi_i);

        // One ring model/Clarke's model plus f-selectivity according to Cavers:
        // Due to isotropic antenna gain pattern on all paths only a^2 can be received on all paths.
        // Since we are interested in attenuation a := 1, attenuation per path is then:
        double attenuation = (1.00 / sqrt(static_cast<double>(cp.numFadingPaths)));

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

double RadioMedium::computeSpeed(StochasticChannelModel *radio, MacNodeId nodeId, const inet::Coord coord)
{
    double speed = 0.0;
    PerRadioStochasticState& state = stateOf(radio);

    // createIfMissing=false: a node with no history yet must stay absent,
    // not gain an empty placeholder queue that a later front()/back() would
    // read as an entry
    std::queue<Position> *history = positionHistory(state, nodeId, false);

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

double RadioMedium::computeCorrelationDistance(StochasticChannelModel *radio, const LinkKey& key, const inet::Coord coord)
{
    double dist = 0.0;
    PerRadioStochasticState& state = stateOf(radio);

    bool existed = false;
    Position& point = correlationPoint(state, key, &existed);

    if (!existed) {
        // no lastCorrelationPoint found. Add current position and return dist = 0.0
        point = Position(NOW, coord);
    }
    else {
        dist = point.second.distance(coord);
    }
    return dist;
}

} //namespace
