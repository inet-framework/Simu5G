//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"

#include <fstream>
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/mac/amc/UserTxParams.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/nodes/ExtCell.h"
#include "simu5g/background/cell/BackgroundScheduler.h"
#include "simu5g/stack/phy/LtePhyUe.h"
#include "simu5g/stack/mac/LteMacEnbD2D.h"

namespace simu5g {

// attenuation value to be returned if the maximum distance of a scenario has been violated
// and tolerating the maximum distance violation is enabled
#define ATT_MAXDISTVIOLATED    1000

using namespace inet;
using namespace omnetpp;
Define_Module(LteRealisticChannelModel);

simsignal_t LteRealisticChannelModel::rcvdSinrDlSignal_ = registerSignal("rcvdSinrDl");
simsignal_t LteRealisticChannelModel::rcvdSinrUlSignal_ = registerSignal("rcvdSinrUl");
simsignal_t LteRealisticChannelModel::rcvdSinrD2DSignal_ = registerSignal("rcvdSinrD2D");

simsignal_t LteRealisticChannelModel::measuredSinrDlSignal_ = registerSignal("measuredSinrDl");
simsignal_t LteRealisticChannelModel::measuredSinrUlSignal_ = registerSignal("measuredSinrUl");

void LteRealisticChannelModel::initialize(int stage)
{
    LteChannelModel::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        scenario_ = aToDeploymentScenario(par("scenario").stringValue());
        hNodeB_ = par("nodebHeight");
        shadowing_ = par("shadowing");
        hBuilding_ = par("buildingHeight");
        inside_building_ = par("insideBuilding");
        if (inside_building_)
            inside_distance_ = uniform(0.0, 25.0);
        tolerateMaxDistViolation_ = par("tolerateMaxDistViolation");
        hUe_ = par("ueHeight");

        wStreet_ = par("streetWidth");

        correlationDistance_ = par("correlationDistance");
        harqReduction_ = par("harqReduction");

        antennaGainUe_ = par("antennaGainUe");
        antennaGainEnB_ = par("antennGainEnB");
        antennaGainMicro_ = par("antennGainMicro");
        thermalNoise_ = par("thermalNoise");
        cableLoss_ = par("cableLoss");
        ueNoiseFigure_ = par("ueNoiseFigure");
        bsNoiseFigure_ = par("bsNoiseFigure");
        useTorus_ = par("useTorus");
        dynamicLos_ = par("dynamicLos");
        fixedLos_ = par("fixedLos");

        fading_ = par("fading");
        std::string fType = par("fadingType");
        if (fType == "JAKES")
            fadingType_ = JAKES;
        else if (fType == "RAYLEIGH")
            fadingType_ = RAYLEIGH;
        else
            throw cRuntimeError("Unrecognized value in 'fadingType' parameter: \"%s\"", fType.c_str());

        fadingPaths_ = par("numFadingPaths");
        enableBackgroundCellInterference_ = par("bgCellInterference");
        enableExtCellInterference_ = par("extCellInterference");
        enableDownlinkInterference_ = par("downlinkInterference");
        enableUplinkInterference_ = par("uplinkInterference");
        enableD2DInterference_ = par("d2dInterference");
        delayRMS_ = par("delayRms");

        enable_extCell_los_ = par("enableExtCellLos");

        collectSinrStatistics_ = par("collectSinrStatistics");

        //clear jakes fading map structure
        jakesFadingMap_.clear();
    }
}

RadioLink LteRealisticChannelModel::cellularLink(MacNodeId ueId, Direction dir, Coord coord, bool cqiDl)
{
    // The local module is one endpoint and 'coord' the other; 'dir' says which
    // of the two is the UE. The UE is the node whose channel state we track.
    RadioLink link;
    link.dir = dir;
    link.stateKey = ueId;
    link.useUeSideMaps = cqiDl;

    if (dir == DL) { // the local module is the UE, 'coord' is the BS
        link.txIsBaseStation = true;
        link.txCoord = coord;
        link.rxCoord = phy_->getCoord();
        link.stateCoord = phy_->getCoord();
        link.rxId = ueId;
    }
    else { // the local module is the BS, 'coord' is the UE
        link.txIsBaseStation = false;
        link.txCoord = coord;
        link.rxCoord = phy_->getCoord();
        link.stateCoord = coord;
        link.txId = ueId;
    }
    return link;
}

RadioLink LteRealisticChannelModel::linkFor(UserControlInfo *lteInfo)
{
    RadioLink link;
    link.dir = lteInfo->getDirection();

    // The object associated with the packet: the eNodeB if the direction is DL,
    // the UE if it is UL.
    Coord coord = lteInfo->getCoord();

    MacNodeId ueId, eNbId;
    Coord ueCoord, enbCoord;

    /*
     * If the direction is DL and this is not a feedback packet, this function has been
     * called by isReceptionSuccessful() in the UE: downlink error computation.
     */
    if (link.dir == DL && (lteInfo->getFrameType() != FEEDBACKPKT)) {
        ueId = lteInfo->getDestId();
        eNbId = lteInfo->getSourceId();
        ueCoord = phy_->getCoord();
        enbCoord = coord;
        link.useUeSideMaps = false;
    }
    /*
     * If the direction is UL, or the packet is a feedback packet, this function is called
     * by the feedback computation module located in the eNodeB, which computes the feedback
     * received from the UE. Hence the UE macNodeId comes from the sourceId of the lteInfo.
     */
    else { // UL/DL CQI & UL error computation
        ueId = lteInfo->getSourceId();
        eNbId = lteInfo->getDestId();
        ueCoord = coord;
        enbCoord = phy_->getCoord();
        // for a DL CQI we need the maps stored on the UE side
        link.useUeSideMaps = (link.dir == DL);
    }

    if (link.dir == DL) {
        link.noiseFigure = ueNoiseFigure_;    // dB
        link.txAntennaGain = antennaGainEnB_; // dB
        link.rxAntennaGain = antennaGainUe_;  // dB
        link.txIsBaseStation = true;
        link.txId = eNbId;
        link.rxId = ueId;
        link.txCoord = enbCoord;
        link.rxCoord = ueCoord;
    }
    else { // if( dir == UL )
        // TODO check if antennaGainEnB should be added in UL direction too
        link.txAntennaGain = antennaGainUe_;
        link.rxAntennaGain = antennaGainEnB_;
        link.noiseFigure = bsNoiseFigure_;
        link.txIsBaseStation = false;
        link.txId = ueId;
        link.rxId = eNbId;
        link.txCoord = ueCoord;
        link.rxCoord = enbCoord;
    }

    // The UE owns the channel state, and it is always the UE's position that feeds
    // the speed and correlation-distance computation -- which is why the old code's
    // "pass UL for a FEEDBACKPKT" special case is not needed here: it only existed
    // to make getAttenuation() pick 'coord' rather than phy_->getCoord().
    link.stateKey = ueId;
    link.stateCoord = ueCoord;

    // the cell this link belongs to, for the interference computation
    link.cellId = eNbId;

    return link;
}

double LteRealisticChannelModel::getAttenuation(const RadioLink& link)
{
    //COMPUTE DISTANCE between the two endpoints
    double sqrDistance = link.txCoord.distance(link.rxCoord);

    double speed = computeSpeed(link.stateKey, link.stateCoord);
    double correlationDist = computeCorrelationDistance(link.stateKey, link.stateCoord);

    // If Euclidean distance since last LOS probability computation is greater than
    // correlation distance the UE could have changed its state and
    // its visibility from eNodeB, hence it is correct to recompute the LOS probability
    if (correlationDist > correlationDistance_
        || losMap_.find(link.stateKey) == losMap_.end())
    {
        computeLosProbability(sqrDistance, link.stateKey);
    }

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losMap_[link.stateKey];
    double dbp = 0;
    double attenuation = computePathLoss(sqrDistance, dbp, los);

    //    Applying shadowing only if it is enabled by configuration
    //    log-normal shadowing (not available for background UEs)
    if (num(link.stateKey) < BGUE_MIN_ID && shadowing_)
        attenuation += computeShadowing(sqrDistance, link.stateKey, speed, link.useUeSideMaps);

    // update the tracked node's current position
    updatePositionHistory(link.stateKey, link.stateCoord);
    updateCorrelationDistance(link.stateKey, link.stateCoord);

    EV << "LteRealisticChannelModel::getAttenuation - computed attenuation at distance " << sqrDistance << " for eNB is " << attenuation << endl;

    return attenuation;
}

RadioLink LteRealisticChannelModel::d2dLink(MacNodeId srcId, Coord srcCoord, MacNodeId destId, Coord destCoord, bool useUeSideMaps)
{
    RadioLink link;
    link.dir = D2D;

    link.txId = srcId;
    link.rxId = destId;
    link.txCoord = srcCoord;
    link.rxCoord = destCoord;

    // Both endpoints are UEs.
    link.txAntennaGain = link.rxAntennaGain = antennaGainUe_;
    link.noiseFigure = ueNoiseFigure_;
    link.txIsBaseStation = false; // omnidirectional: no angular attenuation

    // The channel state is keyed on the transmitter, which is what this code has
    // always done. See the note on RadioLink::stateKey: it ought to be a link key,
    // not a node key, so that a UE's several D2D peers do not share one slot --
    // deliberately left unchanged here, as fixing it moves every stored fading and
    // shadowing realization.
    link.stateKey = srcId;
    link.stateCoord = srcCoord;
    link.useUeSideMaps = useUeSideMaps;

    return link;
}

double LteRealisticChannelModel::computeShadowing(double sqrDistance, MacNodeId nodeId, double speed, bool cqiDl)
{
    ShadowFadingMap *actualShadowingMap;

    if (cqiDl) // if we are computing a DL CQI we need the Shadowing Map stored on the UE side
        actualShadowingMap = obtainShadowingMap(nodeId);
    else
        actualShadowingMap = &lastComputedSF_;

    if (actualShadowingMap == nullptr)
        throw cRuntimeError("LteRealisticChannelModel::computeShadowing - actualShadowingMap not found (nullptr)");

    double mean = 0;
    double dbp = 0.0;
    //Get std deviation according to LOS/NLOS and selected scenario

    double stdDev = getStdDev(sqrDistance < dbp, nodeId);
    double time = 0;
    double space = 0;
    double att;

    // if direction is DOWNLINK it means that this module is located in the UE stack than
    // the Move object associated with the UE is myMove_ variable
    // if direction is UPLINK it means that this module is located in the UE stack than
    // the Move object associated with the UE is move variable

    // if shadowing for current user has never been computed
    if (actualShadowingMap->find(nodeId) == actualShadowingMap->end()) {
        //Get the log-normal shadowing with std deviation stdDev
        att = normal(mean, stdDev);

        //store the shadowing attenuation for this user and the temporal mark
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[nodeId] = tmp;

        //If the shadowing attenuation has been computed at least one time for this user
        // and the distance traveled by the UE is greater than correlation distance
    }
    else if ((NOW - actualShadowingMap->at(nodeId).first).dbl() * speed
             > correlationDistance_)
    {

        //get the temporal mark of the last computed shadowing attenuation
        time = (NOW - actualShadowingMap->at(nodeId).first).dbl();

        //compute the traveled distance
        space = time * speed;

        //Compute shadowing with an EAW (Exponential Average Window) (step 1)
        double a = exp(-0.5 * (space / correlationDistance_));

        //Get last shadowing attenuation computed
        double old = actualShadowingMap->at(nodeId).second;

        //Compute shadowing with an EAW (Exponential Average Window) (step 2)
        att = a * old + sqrt(1 - pow(a, 2)) * normal(mean, stdDev);

        // Store the new computed shadowing
        std::pair<simtime_t, double> tmp(NOW, att);
        (*actualShadowingMap)[nodeId] = tmp;

        // if the distance traveled by the UE is smaller than correlation distance shadowing attenuation remains the same
    }
    else {
        att = actualShadowingMap->at(nodeId).second;
    }

    return att;
}

void LteRealisticChannelModel::updatePositionHistory(const MacNodeId nodeId,
        const Coord coord)
{
    if (positionHistory_.find(nodeId) != positionHistory_.end()) {
        // position already updated for this TTI.
        if (positionHistory_[nodeId].back().first == NOW)
            return;
    }

    // FIXME: possible memory leak
    positionHistory_[nodeId].push(Position(NOW, coord));

    if (positionHistory_[nodeId].size() > 2) // if we have more than a past and a current element
        // drop the oldest one
        positionHistory_[nodeId].pop();
}

void LteRealisticChannelModel::updateCorrelationDistance(const MacNodeId nodeId, const inet::Coord coord) {

    if (lastCorrelationPoint_.find(nodeId) == lastCorrelationPoint_.end()) {
        // no lastCorrelationPoint set current point.
        lastCorrelationPoint_[nodeId] = Position(NOW, coord);
    }
    else if ((lastCorrelationPoint_[nodeId].first != NOW) &&
             lastCorrelationPoint_[nodeId].second.distance(coord) > correlationDistance_)
    {
        // check simtime_t first
        lastCorrelationPoint_[nodeId] = Position(NOW, coord);
    }
}

double LteRealisticChannelModel::computeCorrelationDistance(const MacNodeId nodeId, const inet::Coord coord) {
    double dist = 0.0;

    if (lastCorrelationPoint_.find(nodeId) == lastCorrelationPoint_.end()) {
        // no lastCorrelationPoint found. Add current position and return dist = 0.0
        lastCorrelationPoint_[nodeId] = Position(NOW, coord);
    }
    else {
        dist = lastCorrelationPoint_[nodeId].second.distance(coord);
    }
    return dist;
}

double LteRealisticChannelModel::computeSpeed(const MacNodeId nodeId,
        const Coord coord)
{
    double speed = 0.0;

    if (positionHistory_.find(nodeId) == positionHistory_.end()) {
        // no entries
        return speed;
    }
    else {
        //compute distance traveled from last update by UE (eNodeB position is fixed)

        if (positionHistory_[nodeId].size() == 1) {
            //  the only element refers to the present, return 0
            return speed;
        }

        double movement = positionHistory_[nodeId].front().second.distance(coord);

        if (movement <= 0.0)
            return speed;
        else {
            double time = (NOW.dbl()) - (positionHistory_[nodeId].front().first.dbl());
            if (time <= 0.0) // time not updated since last speed call
                throw cRuntimeError("Multiple entries detected in position history referring to the same time");
            // compute speed
            speed = (movement) / (time);
        }
    }
    return speed;
}

double LteRealisticChannelModel::computeAngle(Coord center, Coord point) {
    double relx, rely, arcoSen, angle, dist;

    // compute distance between points
    dist = point.distance(center);

    // compute distance along the axis
    relx = point.x - center.x;
    rely = point.y - center.y;

    // compute the arc sine
    arcoSen = asin(rely / dist) * 180.0 / M_PI;

    // adjust the angle depending on the quadrants
    if (relx < 0 && rely > 0) // quadrant II
        angle = 180.0 - arcoSen;
    else if (relx < 0 && rely <= 0) // quadrant III
        angle = 180.0 - arcoSen;
    else if (relx > 0 && rely < 0) // quadrant IV
        angle = 360.0 + arcoSen;
    else
        // quadrant I
        angle = arcoSen;

    return angle;
}

double LteRealisticChannelModel::computeVerticalAngle(Coord center, Coord point)
{
    double threeDimDistance = center.distance(point);
    double twoDimDistance = getTwoDimDistance(center, point);
    double arccos = acos(twoDimDistance / threeDimDistance) * 180.0 / M_PI;
    return 90 + arccos;
}

double LteRealisticChannelModel::computeAngularAttenuation(double hAngle, double vAngle) {

    // in this implementation, vertical angle is not considered

    double angularAtt;
    double angularAttMin = 25;
    // compute attenuation due to angular position
    // see TR 36.814 V9.0.0 for more details
    angularAtt = 12 * pow(hAngle / 70.0, 2);

    //  EV << "\t angularAtt[" << angularAtt << "]" << endl;
    // max value for angular attenuation is 25 dB
    if (angularAtt > angularAttMin)
        angularAtt = angularAttMin;

    return angularAtt;
}

std::vector<double> LteRealisticChannelModel::getSINR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    RadioLink link = linkFor(lteInfo);

    EV << "------------ GET SINR ----------------" << endl;

    // The desired signal: path loss, shadowing and fading. getSINR() below adds
    // noise and interference on top of it.
    return getSINR(link, lteInfo, getRSRP(link, lteInfo->getTxPower()));
}

std::vector<double> LteRealisticChannelModel::getSINR(const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector)
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
     * Ndb = thermalNoise_ + noiseFigure (measured in decibels)
     */

    // compute and linearize total noise
    double totN = dBmToLinear(thermalNoise_ + link.noiseFigure);

    // per-band interference-plus-noise denominator, in dBm
    std::vector<double> den(numBands_, 0.0);
    computeInterferencePlusNoise(link, lteInfo, rbmap, totN, den);

    double sumSnr = 0.0;
    int usedRBs = 0;
    for (unsigned int i = 0; i < numBands_; i++) {
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
    if (collectSinrStatistics_ && (lteInfo->getFrameType() == FEEDBACKPKT) && usedRBs > 0
        && (link.dir == DL || link.dir == UL))
    {
        // we are on the BS, so we need to retrieve the channel model of the sender
        // XXX I know, there might be a faster way...
        LteChannelModel *ueChannelModel = check_and_cast<LtePhyUe *>(binder_->getPhyByNodeId(ueId))->getChannelModel(lteInfo->getCarrierFrequency());

        if (link.dir == DL) // we are on the UE
            ueChannelModel->emit(measuredSinrDlSignal_, sumSnr / usedRBs);
        else
            ueChannelModel->emit(measuredSinrUlSignal_, sumSnr / usedRBs);
    }

    // if sender is an eNodeB
    if (link.dir == DL)
        // store the position of user
        updatePositionHistory(ueId, phy_->getCoord());
    // sender is a UE
    else
        updatePositionHistory(ueId, lteInfo->getCoord());
    return snrVector;
}

void LteRealisticChannelModel::computeInterferencePlusNoise(const RadioLink& link, UserControlInfo *lteInfo,
        RbMap& rbmap, double totN, std::vector<double>& den)
{
    // A UE-to-UE link is not described by the cellular interference model at all;
    // it has its own. Once the D2D code moves out of this class this branch becomes
    // an override of this function.
    if (link.dir == D2D || link.dir == D2D_MULTI) {
        computeD2DInterferencePlusNoise(link, lteInfo, rbmap, totN, den);
        return;
    }

    // The interference model is cellular-topology-aware (it asks "which cell?"),
    // so recover the UE/BS roles from the link. The propagation math does not need them.
    Direction dir = link.dir;
    MacNodeId ueId = link.txIsBaseStation ? link.rxId : link.txId;
    MacNodeId eNbId = link.cellId;
    Coord ueCoord = link.txIsBaseStation ? link.rxCoord : link.txCoord;
    Coord enbCoord = link.txIsBaseStation ? link.txCoord : link.rxCoord;

    //============ MULTI CELL INTERFERENCE COMPUTATION =================
    // vector containing the sum of multi-cell interference for each band
    std::vector<double> multiCellInterference; // Linear value (mW)
    // prepare data structure
    multiCellInterference.resize(numBands_, 0);
    if (enableDownlinkInterference_ && dir == DL && lteInfo->getFrameType() != BEACONPKT) {
        computeDownlinkInterference(eNbId, ueId, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }
    else if (enableUplinkInterference_ && dir == UL) {
        computeUplinkInterference(eNbId, ueId, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }

    //============ BACKGROUND CELLS INTERFERENCE COMPUTATION =================
    // vector containing the sum of background cell interference for each band
    std::vector<double> bgCellInterference; // Linear value (mW)
    // prepare data structure
    bgCellInterference.resize(numBands_, 0);
    if (enableBackgroundCellInterference_) {
        computeBackgroundCellInterference(ueId, enbCoord, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, dir, &bgCellInterference); // dBm
    }

    //============ EXTCELL INTERFERENCE COMPUTATION =================
    // TODO this might be obsolete as it is replaced by background cell interference
    // vector containing the sum of external cell interference for each band
    std::vector<double> extCellInterference; // Linear value (mW)
    // prepare data structure
    extCellInterference.resize(numBands_, 0);
    if (enableExtCellInterference_ && dir == DL) {
        computeExtCellInterference(eNbId, ueId, ueCoord, (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), &extCellInterference); // dBm
    }

    EV << "LteRealisticChannelModel::getSINR - distance from my eNb=" << enbCoord.distance(ueCoord) << " - DIR=" << ((dir == DL) ? "DL" : "UL") << endl;

    for (unsigned int i = 0; i < numBands_; i++) {
        // the caller skips these bands too; leave their denominator untouched
        if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
            continue;

        //                  (      mW              +          mW            +  mW  +        mW            )
        den[i] = linearToDBm(bgCellInterference[i] + extCellInterference[i] + totN + multiCellInterference[i]);

        EV << "\t bgCell[" << bgCellInterference[i] << "] - ext[" << extCellInterference[i] << "] - multi[" << multiCellInterference[i]
           << "] - den[" << den[i] << "]\n";
    }
}

std::vector<double> LteRealisticChannelModel::getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return getRSRP(linkFor(lteInfo), lteInfo->getTxPower());
}

std::vector<double> LteRealisticChannelModel::getRSRP(const RadioLink& link, double txPower)
{
    double recvPower = txPower; // dBm

    EV << "LteRealisticChannelModel::getRSRP - txId=" << link.txId
       << " - rxId=" << link.rxId
       << " - DIR=" << dirToA(link.dir)
       << " - txPwr " << txPower
       << " - txCoord[" << link.txCoord << "] - rxCoord[" << link.rxCoord << "]" << endl;

    // =============== PATH LOSS + SHADOWING + FADING =================
    EV << "\t using parameters - noiseFigure=" << link.noiseFigure
       << " - antennaGainTx=" << link.txAntennaGain << " - antennaGainRx=" << link.rxAntennaGain
       << " - txPwr=" << txPower << " - for nodeId=" << link.stateKey << endl;

    // Speed must be read BEFORE getAttenuation(), which appends to positionHistory_:
    // computeSpeed() derives from that history, so evaluating it afterwards would
    // yield a different value and hence different fading. Load-bearing ordering.
    double speed = computeSpeed(link.stateKey, link.stateCoord);

    // attenuation for the desired signal
    double attenuation = getAttenuation(link); // dB

    // compute attenuation (PATHLOSS + SHADOWING)
    recvPower -= attenuation; // (dBm-dB)=dBm

    // add antenna gain
    recvPower += link.txAntennaGain; // (dBm+dB)=dBm
    recvPower += link.rxAntennaGain; // (dBm+dB)=dBm

    // sub cable loss
    recvPower -= cableLoss_; // (dBm-dB)=dBm

    // =============== ANGULAR ATTENUATION =================
    // Only a base station has a sectorial antenna; a UE-to-UE link never gets here.
    if (link.txIsBaseStation) {
        cModule *eNbModule = binder_->getNodeModule(link.txId);
        LtePhyBase *ltePhy = eNbModule ?
            check_and_cast<LtePhyBase *>(eNbModule->getSubmodule("cellularNic")->getSubmodule("phy")) :
            nullptr;

        if (ltePhy && ltePhy->getTxDirection() == ANISOTROPIC) {
            // get tx angle
            double txAngle = ltePhy->getTxAngle();

            // compute the angle between the receiver position and the reference axis,
            // considering the transmitting BS as center
            double ueAngle = computeAngle(link.txCoord, link.rxCoord);

            // compute the reception angle
            double recvAngle = fabs(txAngle - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(link.txCoord, link.rxCoord);

            // compute attenuation due to sectorial tx
            double angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);

            recvPower -= angularAtt;
        }
        // else, antenna is omni-directional
    }
    // =============== END ANGULAR ATTENUATION =================

    std::vector<double> rsrpVector;
    rsrpVector.resize(numBands_, 0.0);

    // compute and add interference due to fading
    // Apply fading for each band
    // if the phy layer is localized we can assume that for each logical band we have different fading attenuation
    // if the phy layer is distributed the number of logical bands should be set to 1
    double fadingAttenuation = 0;

    // for each logical band
    // FIXME compute fading only for used RBs
    for (unsigned int i = 0; i < numBands_; i++) {
        fadingAttenuation = 0;
        // if fading is enabled
        if (fading_) {
            // Applying fading
            if (fadingType_ == RAYLEIGH)
                fadingAttenuation = rayleighFading(link.stateKey, i);

            else if (fadingType_ == JAKES)
                fadingAttenuation = jakesFading(link.stateKey, speed, i, link.useUeSideMaps);
        }
        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        EV << " LteRealisticChannelModel::getRSRP node " << link.stateKey
           << " band " << i << " recvPower " << recvPower
           << " direction " << dirToA(link.dir) << " antenna gain tx "
           << link.txAntennaGain << " antenna gain rx " << link.rxAntennaGain
           << " noise figure " << link.noiseFigure
           << " cable loss   " << cableLoss_
           << " attenuation (pathloss + shadowing) " << attenuation
           << " speed " << speed << " thermal noise " << thermalNoise_
           << " fading attenuation " << fadingAttenuation << endl;

        rsrpVector[i] = finalRecvPower;
    }
    // ============ END PATH LOSS + SHADOWING + FADING ===============

    return rsrpVector;
}

std::vector<double> LteRealisticChannelModel::getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    //get tx power
    double recvPower = lteInfo->getTxPower(); // dBm

    // get MacId and Direction
    MacNodeId bgUeId = lteInfo->getSourceId();
    MacNodeId eNbId = lteInfo->getDestId();
    Direction dir = lteInfo->getDirection();

    // position of e/gNb and UE
    Coord ueCoord = lteInfo->getCoord();
    Coord enbCoord = phy_->getCoord();

    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;
    double noiseFigure = 0.0;
    double speed = 0.0;

    // true if we are computing a CQI for the DL direction
    bool cqiDl = false;

    EV << "------------ GET SINR for background UE ----------------" << endl;
    //===================== PARAMETERS SETUP ============================
    /*
     * This function is called on the e/gNodeB side and is similar
     * to what is called when computing feedback
     */
    if (dir == DL) {
        //set noise figure
        noiseFigure = ueNoiseFigure_; //dB
        //set antenna gain figure
        antennaGainTx = antennaGainEnB_; //dB
        antennaGainRx = antennaGainUe_;  //dB
        // use the jakes map on the UE side
        cqiDl = true;
    }
    else { // if( dir == UL )
        // TODO check if antennaGainEnB should be added in UL direction too
        antennaGainTx = antennaGainUe_;
        antennaGainRx = antennaGainEnB_;
        noiseFigure = bsNoiseFigure_;
        // use the jakes map on the eNb side
        cqiDl = false;
    }
    speed = computeSpeed(bgUeId, ueCoord);

    CellInfo *eNbCell = binder_->getCellInfoByNodeId(eNbId);
    const char *eNbTypeString = eNbCell ? (eNbCell->getEnbType() == MACRO_ENB ? "MACRO" : "MICRO") : "NULL";

    EV << "LteRealisticChannelModel::getSINR_bgUe - DIR=" << ((dir == DL) ? "DL" : "UL")
       << " " << eNbTypeString << " - txPwr " << lteInfo->getTxPower()
       << " - ueCoord[" << ueCoord << "] - enbCoord[" << enbCoord << "] - enbId[" << eNbId << "]" <<
        endl;

    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS =================
    // Note that shadowing and fading effects are not applied here and left FFW

    // UL because we are computing a feedback
    double attenuation = getAttenuation(bgUeId, UL, ueCoord, cqiDl);

    //compute recvPower
    recvPower -= attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm
    //sub cable loss
    recvPower -= cableLoss_; // (dBm-dB)=dBm

    // ANGULAR ATTENUATION
    if (dir == DL) {
        //get tx angle
        cModule *eNbModule = binder_->getNodeModule(eNbId);
        LtePhyBase *ltePhy = eNbModule ?
            check_and_cast<LtePhyBase *>(eNbModule->getSubmodule("cellularNic")->getSubmodule("phy")) :
            nullptr;

        if (ltePhy && ltePhy->getTxDirection() == ANISOTROPIC) {
            // get tx angle
            double txAngle = ltePhy->getTxAngle();

            // compute the angle between uePosition and reference axis, considering the eNb as center
            double ueAngle = computeAngle(enbCoord, ueCoord);

            // compute the reception angle between ue and eNb
            double recvAngle = fabs(txAngle - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(enbCoord, ueCoord);

            // compute attenuation due to sectorial tx
            double angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);

            recvPower -= angularAtt;
        }
        // else, antenna is omni-directional
    }

    std::vector<double> snrVector;
    snrVector.resize(numBands_, recvPower);

    // for each logical band
    double fadingAttenuation = 0;
    for (unsigned int i = 0; i < numBands_; i++) {
        //if fading is enabled
        if (fading_) {
            //Applying fading
            if (fadingType_ == RAYLEIGH)
                fadingAttenuation = rayleighFading(bgUeId, i);

            else if (fadingType_ == JAKES)
                fadingAttenuation = jakesFading(bgUeId, speed, i, cqiDl, true);
        }
        // add fading contribution to the received power
        double finalRecvPower = recvPower + fadingAttenuation; // (dBm+dB)=dBm

        snrVector[i] = finalRecvPower;
    }

    //============ END PATH LOSS + SHADOWING + FADING ===============

    /*
     * The SINR will be calculated as follows
     *
     *           Pwr
     * SINR = ---------
     *         N  +  I
     *
     * Ndb = thermalNoise_ + noiseFigure (measured in decibel)
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
    multiCellInterference.resize(numBands_, 0);
    if (enableDownlinkInterference_ && dir == DL) {
        computeDownlinkInterference(eNbId, bgUeId, ueCoord, isCqi, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }
    else if (enableUplinkInterference_ && dir == UL) {
        computeUplinkInterference(eNbId, bgUeId, isCqi, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    }

    //============ BACKGROUND CELLS INTERFERENCE COMPUTATION =================
    //vector containing the sum of bg-cell interference for each band
    std::vector<double> bgCellInterference; // Linear value (mW)
    // prepare data structure
    bgCellInterference.resize(numBands_, 0);
    if (enableBackgroundCellInterference_) {
        computeBackgroundCellInterference(bgUeId, enbCoord, ueCoord, isCqi, lteInfo->getCarrierFrequency(), rbmap, dir, &bgCellInterference); // dBm
    }

    //============ EXTCELL INTERFERENCE COMPUTATION =================
    // TODO this might be obsolete as it is replaced by background cell interference
    //vector containing the sum of ext-cell interference for each band
    std::vector<double> extCellInterference; // Linear value (mW)
    // prepare data structure
    extCellInterference.resize(numBands_, 0);
    if (enableExtCellInterference_ && dir == DL) {
        computeExtCellInterference(eNbId, bgUeId, ueCoord, isCqi, lteInfo->getCarrierFrequency(), &extCellInterference); // dBm
    }

    //===================== SINR COMPUTATION ========================
    // compute and linearize total noise
    double totN = dBmToLinear(thermalNoise_ + noiseFigure);

    // add interference for each band
    for (unsigned int i = 0; i < numBands_; i++) {
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

double LteRealisticChannelModel::getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId)
{
    double antennaGainTx = 0.0;
    double antennaGainRx = 0.0;

    EV << NOW << " LteRealisticChannelModel::getReceivedPower_bgUe" << endl;

    //===================== PARAMETERS SETUP ============================
    if (dir == DL) {
        antennaGainTx = antennaGainEnB_; //dB
        antennaGainRx = antennaGainUe_;  //dB
    }
    else { // if( dir == UL )
        antennaGainTx = antennaGainUe_;
        antennaGainRx = antennaGainEnB_;
    }

    EV << "LteRealisticChannelModel::getReceivedPower_bgUe - DIR=" << ((dir == DL) ? "DL" : "UL")
       << " - txPwr " << txPower << " - txPos[" << txPos << "] - rxPos[" << rxPos << "] " << endl;
    //=================== END PARAMETERS SETUP =======================

    //=============== PATH LOSS =================
    // Note that shadowing and fading effects are not applied here and left FFW

    //compute attenuation based on selected scenario and based on LOS or NLOS
    double sqrDistance = txPos.distance(rxPos);
    double dbp = 0;
    double attenuation = computePathLoss(sqrDistance, dbp, losStatus);

    //compute recvPower
    double recvPower = txPower - attenuation; // (dBm-dB)=dBm

    //add antenna gain
    recvPower += antennaGainTx; // (dBm+dB)=dBm
    recvPower += antennaGainRx; // (dBm+dB)=dBm
    //sub cable loss
    recvPower -= cableLoss_; // (dBm-dB)=dBm

    // ANGULAR ATTENUATION
    if (dir == DL) {
        //get tx angle
        cModule *bsModule = binder_->getNodeModule(bsId);
        LtePhyBase *phy = bsModule ? check_and_cast<LtePhyBase *>(bsModule->getSubmodule("cellularNic")->getSubmodule("phy")) : nullptr;

        if (phy && phy->getTxDirection() == ANISOTROPIC) {
            // get tx angle
            double txAngle = phy->getTxAngle();

            // compute the angle between uePosition and reference axis, considering the eNb as center
            double ueAngle = computeAngle(txPos, rxPos);

            // compute the reception angle between ue and eNb
            double recvAngle = fabs(txAngle - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(txPos, rxPos);

            // compute attenuation due to sectorial tx
            double angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);

            recvPower -= angularAtt;
        }
        // else, antenna is omni-directional
    }
    //============ END PATH LOSS + ANGULAR ATTENUATION ===============

    return recvPower;
}

std::vector<double> LteRealisticChannelModel::getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, Coord destCoord)
{
    EV << "------------ GET RSRP D2D----------------" << endl;

    // D2D is like DL for the receivers, so the UE-side fading/shadowing maps apply.
    RadioLink link = d2dLink(lteInfo_1->getSourceId(), lteInfo_1->getCoord(), destId, destCoord, true);

    // Note the D2D-specific transmit power: a D2D transmission does not use the
    // power the UE would use towards the base station.
    return getRSRP(link, lteInfo_1->getD2dTxPower());
}

std::vector<double> LteRealisticChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, Coord destCoord, MacNodeId enbId)
{
    // desired-signal RSRP (pathloss + shadowing + fading), then noise and
    // interference on top: exactly the two halves this body used to inline
    std::vector<double> rsrpVector = getRSRP_D2D(frame, lteInfo, destId, destCoord);
    return getSINR_D2D(frame, lteInfo, destId, destCoord, enbId, rsrpVector);
}

std::vector<double> LteRealisticChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector)
{
    EV << "------------ GET SINR D2D----------------" << endl;

    // The desired signal is already known; the core adds noise and interference,
    // asking computeInterferencePlusNoise() below for the D2D denominator.
    RadioLink link = d2dLink(lteInfo_1->getSourceId(), lteInfo_1->getCoord(), destId, destCoord, true);
    link.cellId = enbId;

    // The caller is expected to supply one RSRP value per band. The one-to-many
    // capture-effect path only fills bestRsrpVector_ when the capture factor is
    // "RSRP"; with "Distance" it stays empty, and indexing it below would be out
    // of bounds. Fall back to computing the desired signal here rather than
    // reading past the end.
    if (rsrpVector.size() < numBands_) {
        if (!rsrpVector.empty())
            throw cRuntimeError("LteRealisticChannelModel::getSINR_D2D - RSRP vector has %zu entries, expected %u",
                    rsrpVector.size(), numBands_);
        return getSINR(link, lteInfo_1, getRSRP(link, lteInfo_1->getD2dTxPower()));
    }

    return getSINR(link, lteInfo_1, rsrpVector);
}

void LteRealisticChannelModel::computeD2DInterferencePlusNoise(const RadioLink& link, UserControlInfo *lteInfo,
        RbMap& rbmap, double totN, std::vector<double>& den)
{
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
    // prepare data structure
    d2dInterference.resize(numBands_, 0);
    if (enableD2DInterference_) {
        computeD2DInterference(link.cellId, link.txId, link.txCoord, link.rxId, link.rxCoord,
                (lteInfo->getFrameType() == FEEDBACKPKT), lteInfo->getCarrierFrequency(), rbmap, &d2dInterference, link.dir);
    }

    EV << "LteRealisticChannelModel::computeD2DInterferencePlusNoise - distance from my Peer = "
       << link.rxCoord.distance(link.txCoord) << " - DIR=" << dirToA(link.dir) << endl;

    // One loop for both cases: with interference disabled d2dInterference is all
    // zeros, so this degenerates to the noise-only denominator. (The two used to be
    // written out separately, the disabled branch summing noise directly instead of
    // going through dBm -> linear -> dBm. That round trip is not bit-identical, so
    // the collapse is observable -- but only where d2dInterference is false, which
    // no shipped configuration sets.)
    for (unsigned int i = 0; i < numBands_; i++) {
        // the caller skips these bands too; leave their denominator untouched
        if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
            continue;

        den[i] = linearToDBm(totN + d2dInterference[i]);
        EV << "\t in[" << d2dInterference[i] << "] - den[" << den[i] << "]\n";
    }
}

std::vector<double> LteRealisticChannelModel::getSIR(LteAirFrame *frame,
        UserControlInfo *lteInfo)
{
    // get tx power
    double recvPower = lteInfo->getTxPower();

    Coord coord = lteInfo->getCoord();

    Direction dir = lteInfo->getDirection();

    MacNodeId id = NODEID_NONE;
    double speed = 0.0;

    // if direction is DL
    if (dir == DL && (lteInfo->getFrameType() != FEEDBACKPKT)) {
        id = lteInfo->getDestId();
        speed = computeSpeed(id, phy_->getCoord());
    }
    /*
     * If direction is UL OR
     * if the packet is a feedback packet
     * it means that this function is called by the feedback computation module
     * located in the eNodeB that computes the feedback received by the UE
     * Hence, the UE macNodeId can be taken from the sourceId of the lteInfo
     * and the speed of the UE is contained in the Move object associated with the lteInfo
     */
    else {
        id = lteInfo->getSourceId();
        speed = computeSpeed(id, coord);
    }

    // Apply fading for each band
    // if the phy layer is localized we can assume that for each logical band we have different fading attenuation
    // if the phy layer is distributed, the number of logical bands should be set to 1
    std::vector<double> snrVector;

    double fadingAttenuation = 0;
    // for each logical band
    for (unsigned int i = 0; i < numBands_; i++) {
        fadingAttenuation = 0;
        // if fading is enabled
        if (fading_) {
            // Applying fading
            if (fadingType_ == RAYLEIGH) {
                fadingAttenuation = rayleighFading(id, i);
            }
            else if (fadingType_ == JAKES) {
                fadingAttenuation = jakesFading(id, speed, i, dir);
            }
        }
        // add fading contribution to the final SINR
        double finalSnr = recvPower + fadingAttenuation;

        snrVector.push_back(finalSnr);
    }

    // if sender is an eNodeB
    if (dir == DL)
        // store the position of the user
        updatePositionHistory(id, phy_->getCoord());
    // sender is a UE
    else
        updatePositionHistory(id, coord);
    return snrVector;
}

double LteRealisticChannelModel::rayleighFading(MacNodeId id,
        unsigned int band)
{
    // get rayleigh variable from trace file
    const int channelndex = 0;
    double temp1 = binder_->phyPisaData.getChannel(channelndex + band);
    return linearToDb(temp1);
}

double LteRealisticChannelModel::jakesFading(MacNodeId nodeId, double speed,
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
    JakesFadingMap *actualJakesMap;

    if (cqiDl) // if we are computing a DL CQI we need the Jakes Map stored on the UE side
        actualJakesMap = (!isBgUe) ? obtainUeJakesMap(nodeId) : &jakesFadingMapBgUe_;
    else
        actualJakesMap = &jakesFadingMap_;

    // if this is the first time that we compute fading for current user
    if (actualJakesMap->find(nodeId) == actualJakesMap->end()) {
        // clear the map
        // FIXME: possible memory leak
        (*actualJakesMap)[nodeId].clear();

        // for each band we are going to create a Jakes fading
        for (unsigned int j = 0; j < numBands_; j++) {
            // clear some structure
            JakesFadingData temp;
            temp.angleOfArrival.clear();
            temp.delaySpread.clear();

            // for each fading path
            for (int i = 0; i < fadingPaths_; i++) {
                // get angle of arrivals
                temp.angleOfArrival.push_back(cos(uniform(0, M_PI)));

                // get delay spread
                temp.delaySpread.push_back(exponential(delayRMS_));
            }
            // store the Jakes fading for this user
            (*actualJakesMap)[nodeId].push_back(temp);
        }
    }
    // convert carrier frequency from GHz to Hz
    double f = carrierFrequencyHz_;

    // get transmission time start (TTI = 1ms)
    simtime_t t = simTime().dbl() - 0.001;

    double re_h = 0;
    double im_h = 0;

    const JakesFadingData& actualJakesData = actualJakesMap->at(nodeId).at(band);

    // Compute Doppler shift.
    double doppler_shift = (speed * f) / SPEED_OF_LIGHT;

    for (int i = 0; i < fadingPaths_; i++) {
        // Phase shift due to Doppler => t-selectivity.
        double phi_d = actualJakesData.angleOfArrival[i] * doppler_shift;

        // Phase shift due to delay spread => f-selectivity.
        double phi_i = actualJakesData.delaySpread[i].dbl() * f;

        // Calculate resulting phase due to t-selective and f-selective fading.
        double phi = 2.00 * M_PI * (phi_d * t.dbl() - phi_i);

        // One ring model/Clarke's model plus f-selectivity according to Cavers:
        // Due to isotropic antenna gain pattern on all paths only a^2 can be received on all paths.
        // Since we are interested in attenuation a := 1, attenuation per path is then:
        double attenuation = (1.00 / sqrt(static_cast<double>(fadingPaths_)));

        // Convert to cartesian form and aggregate {Re, Im} over all fading paths.
        re_h = re_h + attenuation * cos(phi);
        im_h = im_h - attenuation * sin(phi);
    }

    // Output: |H_f|^2 = absolute channel impulse response due to fading.
    // Note that this may be >1 due to constructive interference.
    return linearToDb(re_h * re_h + im_h * im_h);
}

bool LteRealisticChannelModel::isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    EV << "LteRealisticChannelModel::error" << endl;

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

    // Take sinr
    std::vector<double> snrV;
    if (lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI) {
        MacNodeId destId = lteInfo->getDestId();
        Coord destCoord = phy_->getCoord();
        MacNodeId enbId = binder_->getServingNodeOrSelf(lteInfo->getSourceId());
        snrV = getSINR_D2D(frame, lteInfo, destId, destCoord, enbId);
    }
    else {
        snrV = getSINR(frame, lteInfo);
    }

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
                return false; // CQI 0 means channel below usable quality (e.g. after handover) — loss
            if (cqi > 15)
                throw cRuntimeError("A packet has been transmitted with a cqi greater than 15 cqi:%d txmode:%d dir:%d rb:%d cw:%d rtx:%d", cqi, lteInfo->getTxMode(), dir, band, cw, transmissionAttempt);

            // for statistical purposes
            sumSnr += snrV[band];
            usedRBs++;

            int snr = snrV[band];// XXX because band is a Band (=unsigned short)
            if (snr < binder_->phyPisaData.minSnr())
                return false;
            else if (snr > binder_->phyPisaData.maxSnr())
                blockErrorRate = 0.0;
            else
                blockErrorRate = binder_->phyPisaData.getBler(itxmode, cqi, snr);

            EV << "\t bler computation: [itxMode=" << itxmode << "] - [cqi=" << cqi
               << "] - [snr=" << snr << "]" << endl;

            double blockSuccessRate = 1.0 - blockErrorRate;
            // compute the success probability according to the number of RB used
            double allocationSuccessProbability = pow(blockSuccessRate, (double)allocation);
            // compute the success probability according to the number of LB used
            cumulativeSuccessProbability *= allocationSuccessProbability;

            EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
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

    EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
       << " node " << id << " total ERROR probability  " << packetErrorRate
       << " per with H-ARQ error reduction " << effectiveErrorRateWithHarq
       << " - CQI[" << cqi << "]- random error extracted[" << randomSample << "]" << endl;

    // emit SINR statistic
    if (collectSinrStatistics_ && usedRBs > 0)
        emitRcvdSinr(dir, id, lteInfo->getCarrierFrequency(), sumSnr / usedRBs);

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

bool LteRealisticChannelModel::isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector)
{
    EV << "LteRealisticChannelModel::error_D2D" << endl;

    // get codeword
    unsigned char cw = lteInfo->getCw();
    // get number of codewords
    int size = lteInfo->getUserTxParams()->readCqiVector().size();

    // if total number of codewords is equal to 1 the cw index should be only 0
    if (size == 1)
        cw = 0;

    // Get CQI used to transmit this cw
    Cqi cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];
    EV << "LteRealisticChannelModel:: CQI: " << cqi << endl;

    MacNodeId id;
    Direction dir = lteInfo->getDirection();

    // Get MacNodeId of UE
    if (dir == DL)
        id = lteInfo->getDestId();
    else // UL or D2D
        id = lteInfo->getSourceId();

    EV << NOW << "LteRealisticChannelModel::FROM: " << id << endl;
    // Get Number of RTX
    unsigned char nTx = lteInfo->getTxNumber();

    // consistency check
    if (nTx == 0)
        throw cRuntimeError("Transmissions counter should not be 0");

    // Get txmode
    TxMode txmode = (TxMode)lteInfo->getTxMode();

    // SINR vector(one SINR value for each band)
    std::vector<double> snrV;
    if (lteInfo->getDirection() == D2D || lteInfo->getDirection() == D2D_MULTI) {
        MacNodeId peerUeMacNodeId = lteInfo->getDestId();
        Coord peerCoord = phy_->getCoord();
        MacNodeId enbId = MacNodeId(1); // TODO get an appropriate way to get EnbId

        if (lteInfo->getDirection() == D2D) {
            snrV = getSINR_D2D(frame, lteInfo, peerUeMacNodeId, peerCoord, enbId);
        }
        else { // D2D_MULTI
            snrV = getSINR_D2D(frame, lteInfo, peerUeMacNodeId, peerCoord, enbId, rsrpVector);
        }
    }
    else snrV = getSINR(frame, lteInfo);                                           // Take SINR

    // Get the resource Block id used to transmit this packet
    RbMap rbmap = lteInfo->getGrantedBlocks();

    // Get txmode
    unsigned int itxmode = txModeToIndex[txmode];

    double bler = 0;
    std::vector<double> totalbler;
    double finalSuccess = 1;

    // for statistical purposes
    double sumSnr = 0.0;
    int usedRBs = 0;

    // for each Remote unit used to transmit the packet
    for (const auto& [remoteUnitId, resourceBlocks] : rbmap) {
        // for each logical band used to transmit the packet
        for (const auto& [band, allocation] : resourceBlocks) {
            // this Rb is not allocated
            if (allocation == 0) continue;

            // Get the Bler
            if (cqi == 0)
                return false; // CQI 0 means channel below usable quality (e.g. after handover) — loss
            if (cqi > 15)
                throw cRuntimeError("A packet has been transmitted with a cqi greater than 15 cqi:%d txmode:%d dir:%d rb:%d cw:%d rtx:%d", cqi, lteInfo->getTxMode(), dir, band, cw, nTx);

            // for statistical purposes
            sumSnr += snrV[band];
            usedRBs++;

            int snr = snrV[band];// XXX because band is a Band (=unsigned short)
            // Use the same usable-SINR floor as the core reception path. This copy
            // used a hardcoded 1 dB, making D2D one-to-many roughly 15 dB stricter
            // than D2D unicast, which reaches the core decision directly.
            if (snr < binder_->phyPisaData.minSnr())
                return false;
            else if (snr > binder_->phyPisaData.maxSnr())
                bler = 0;
            else
                bler = binder_->phyPisaData.getBler(itxmode, cqi, snr);

            EV << "\t bler computation: [itxMode=" << itxmode << "] - [cqi=" << cqi
               << "] - [snr=" << snr << "]" << endl;

            double success = 1 - bler;
            // compute the success probability according to the number of RB used
            double successPacket = pow(success, (double)allocation);

            // compute the success probability according to the number of LB used
            finalSuccess *= successPacket;

            EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
               << " node " << id << " remote unit " << dasToA(remoteUnitId)
               << " Band " << band << " SNR " << snr << " CQI " << cqi
               << " BLER " << bler << " success probability " << successPacket
               << " total success probability " << finalSuccess << endl;
        }
    }
    // Compute total error probability
    double per = 1 - finalSuccess;
    // Harq Reduction
    double totalPer = per * pow(harqReduction_, nTx - 1);

    double er = uniform(0.0, 1.0);

    EV << " LteRealisticChannelModel::error direction " << dirToA(dir)
       << " node " << id << " total ERROR probability  " << per
       << " per with H-ARQ error reduction " << totalPer
       << " - CQI[" << cqi << "]- random error extracted[" << er << "]" << endl;

    // emit SINR statistic
    if (collectSinrStatistics_ && usedRBs > 0)
        emit(rcvdSinrD2DSignal_, sumSnr / usedRBs);

    if (er <= totalPer) {
        EV << "This is NOT your lucky day (" << er << " < " << totalPer << ") -> do not receive." << endl;

        // Signal too weak, we can't receive it
        return false;
    }
    // Signal is strong enough, receive this Signal
    EV << "This is your lucky day (" << er << " > " << totalPer << ") -> Receive AirFrame." << endl;

    return true;
}

void LteRealisticChannelModel::emitRcvdSinr(Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr)
{
    if (dir == D2D || dir == D2D_MULTI) {
        // A D2D reception is not an uplink reception. Attribute it to the receiver --
        // this module -- which is also what the one-to-many path already does, rather
        // than to the sender's channel model the way the UL branch below does.
        emit(rcvdSinrD2DSignal_, sinr);
        return;
    }

    if (dir == DL) { // we are on the UE
        emit(rcvdSinrDlSignal_, sinr);
        return;
    }

    // we are on the BS, so we need to retrieve the channel model of the sender
    // XXX I know, there might be a faster way...
    LteChannelModel *ueChannelModel = check_and_cast<LtePhyUe *>(binder_->getPhyByNodeId(ueId))->getChannelModel(carrierFrequency);
    ueChannelModel->emit(rcvdSinrUlSignal_, sinr);
}

void LteRealisticChannelModel::computeLosProbability(double d,
        MacNodeId nodeId)
{
    double p = 0;
    if (!dynamicLos_) {
        losMap_[nodeId] = fixedLos_;
        return;
    }
    switch (scenario_) {
        case INDOOR_HOTSPOT:
            if (d < 18)
                p = 1;
            else if (d >= 37)
                p = 0.5;
            else
                p = exp((-1) * ((d - 18) / 27));
            break;
        case URBAN_MICROCELL:
            p = (((18 / d) > 1) ? 1 : 18 / d) * (1 - exp(-1 * d / 36))
                + exp(-1 * d / 36);
            break;
        case URBAN_MACROCELL:
            p = (((18 / d) > 1) ? 1 : 18 / d) * (1 - exp(-1 * d / 36))
                + exp(-1 * d / 36);
            break;
        case RURAL_MACROCELL:
            if (d <= 10)
                p = 1;
            else
                p = exp(-1 * (d - 10) / 200);
            break;
        case SUBURBAN_MACROCELL:
            if (d <= 10)
                p = 1;
            else
                p = exp(-1 * (d - 10) / 1000);
            break;
        default:
            throw cRuntimeError("Wrong path-loss scenario value %d", scenario_);
    }
    double random = uniform(0.0, 1.0);
    if (random <= p)
        losMap_[nodeId] = true;
    else
        losMap_[nodeId] = false;
}

double LteRealisticChannelModel::computePathLoss(double distance, double dbp, bool los)
{
    // compute attenuation based on selected scenario and based on LOS or NLOS
    double pathLoss = 0;
    switch (scenario_) {
        case INDOOR_HOTSPOT:
            pathLoss = computeIndoor(distance, los);
            break;
        case URBAN_MICROCELL:
            pathLoss = computeUrbanMicro(distance, los);
            break;
        case URBAN_MACROCELL:
            pathLoss = computeUrbanMacro(distance, los);
            break;
        case RURAL_MACROCELL:
            pathLoss = computeRuralMacro(distance, dbp, los);
            break;
        case SUBURBAN_MACROCELL:
            pathLoss = computeSubUrbanMacro(distance, dbp, los);
            break;
        default:
            throw cRuntimeError("Wrong value %d for path-loss scenario", scenario_);
    }
    return pathLoss;
}

double LteRealisticChannelModel::computeIndoor(double d, bool los)
{
    double a, b;
    if (los) {
        if (d > 150 || d < 3)
            throw cRuntimeError("Error LOS indoor path loss model is valid for 3<d<150");
        a = 16.9;
        b = 32.8;
    }
    else {
        if (d > 250 || d < 6)
            throw cRuntimeError("Error NLOS indoor path loss model is valid for 6<d<250");
        a = 43.3;
        b = 11.5;
    }
    return a * log10(d) + b + 20 * log10CarrierFrequencyGHz_;
}

double LteRealisticChannelModel::computeUrbanMicro(double d, bool los)
{
    if (d < 10)
        d = 10;

    double dbp = 4 * (hNodeB_ - 1) * (hUe_ - 1)
        * (carrierFrequencyHz_ / SPEED_OF_LIGHT);
    if (los) {
        // LOS situation
        if (d > 5000) {
            if (tolerateMaxDistViolation_)
                return ATT_MAXDISTVIOLATED;
            else
                throw cRuntimeError("Error: LOS urban microcell path loss model is valid for d < 5000 m");
        }
        if (d < dbp)
            return 22 * log10(d) + 28 + 20 * log10CarrierFrequencyGHz_;
        else
            return 40 * log10(d) + 7.8 - 18 * log10(hNodeB_ - 1)
                   - 18 * log10(hUe_ - 1) + 2 * log10CarrierFrequencyGHz_;
    }
    // NLOS situation
    if (d < 10)
        throw cRuntimeError("Error: NLOS urban microcell path loss model is valid for 10 m < d ");
    if (d > 5000) {
        if (tolerateMaxDistViolation_)
            return ATT_MAXDISTVIOLATED;
        else
            throw cRuntimeError("Error: NLOS urban microcell path loss model is valid for d < 2000 m");
    }
    return 36.7 * log10(d) + 22.7 + 26 * log10CarrierFrequencyGHz_;
}

double LteRealisticChannelModel::computeUrbanMacro(double d, bool los)
{
    if (d < 10)
        d = 10;

    double dbp = 4 * (hNodeB_ - 1) * (hUe_ - 1)
        * (carrierFrequencyHz_ / SPEED_OF_LIGHT);
    if (los) {
        if (d > 5000) {
            if (tolerateMaxDistViolation_)
                return ATT_MAXDISTVIOLATED;
            else
                throw cRuntimeError("Error: LOS urban macrocell path loss model is valid for d < 5000 m");
        }
        if (d < dbp)
            return 22 * log10(d) + 28 + 20 * log10CarrierFrequencyGHz_;
        else
            return 40 * log10(d) + 7.8 - 18 * log10(hNodeB_ - 1)
                   - 18 * log10(hUe_ - 1) + 2 * log10CarrierFrequencyGHz_;
    }

    if (d < 10)
        throw cRuntimeError("Error: NLOS urban macrocell path loss model is valid for 10 m < d ");
    if (d > 5000) {
        if (tolerateMaxDistViolation_)
            return ATT_MAXDISTVIOLATED;
        else
            throw cRuntimeError("Error: NLOS urban macrocell path loss model is valid for d < 5000 m");
    }

    double att = 161.04 - 7.1 * log10(wStreet_) + 7.5 * log10(hBuilding_)
        - (24.37 - 3.7 * pow(hBuilding_ / hNodeB_, 2)) * log10(hNodeB_)
        + (43.42 - 3.1 * log10(hNodeB_)) * (log10(d) - 3)
        + 20 * log10CarrierFrequencyGHz_
        - (3.2 * (pow(log10(11.75 * hUe_), 2)) - 4.97);
    return att;
}

double LteRealisticChannelModel::computeSubUrbanMacro(double d, double& dbp, bool los)
{
    if (d < 10)
        d = 10;

    dbp = 4 * (hNodeB_ - 1) * (hUe_ - 1)
        * (carrierFrequencyHz_ / SPEED_OF_LIGHT);
    if (los) {
        if (d > 5000) {
            if (tolerateMaxDistViolation_)
                return ATT_MAXDISTVIOLATED;
            else
                throw cRuntimeError("Error: LOS suburban macrocell path loss model is valid for d < 5000 m");
        }
        double a1 = (0.03 * pow(hBuilding_, 1.72));
        double b1 = 0.044 * pow(hBuilding_, 1.72);
        double a = (a1 < 10) ? a1 : 10;
        double b = (b1 < 14.72) ? b1 : 14.72;
        if (d < dbp) {
            double first = 20 * log10((40 * M_PI * d * carrierFrequencyHz_) / 3);
            double second = a * log10(d);
            double fourth = 0.002 * log10(hBuilding_) * d;
            return first + second - b + fourth;
        }
        else
            return 20 * log10((40 * M_PI * dbp * carrierFrequencyHz_) / 3)
                   + a * log10(dbp) - b + 0.002 * log10(hBuilding_) * dbp
                   + 40 * log10(d / dbp);
    }
    if (d > 5000) {
        if (tolerateMaxDistViolation_)
            return ATT_MAXDISTVIOLATED;
        else
            throw cRuntimeError("Error: NLOS suburban macrocell path loss model is valid for 10 < d < 5000 m");
    }
    double att = 161.04 - 7.1 * log10(wStreet_) + 7.5 * log10(hBuilding_)
        - (24.37 - 3.7 * pow(hBuilding_ / hNodeB_, 2)) * log10(hNodeB_)
        + (43.42 - 3.1 * log10(hNodeB_)) * (log10(d) - 3)
        + 20 * log10CarrierFrequencyGHz_
        - (3.2 * (pow(log10(11.75 * hUe_), 2)) - 4.97);
    return att;
}

double LteRealisticChannelModel::computeRuralMacro(double d, double& dbp, bool los)
{
    if (d < 10)
        d = 10;

    dbp = 4 * (hNodeB_ - 1) * (hUe_ - 1)
        * (carrierFrequencyHz_ / SPEED_OF_LIGHT);
    if (los) {
        // LOS situation
        if (d > 10000) {
            if (tolerateMaxDistViolation_)
                return ATT_MAXDISTVIOLATED;
            else
                throw cRuntimeError("Error: LOS rural macrocell path loss model is valid for d < 10000 m");
        }

        double a1 = (0.03 * pow(hBuilding_, 1.72));
        double b1 = 0.044 * pow(hBuilding_, 1.72);
        double a = (a1 < 10) ? a1 : 10;
        double b = (b1 < 14.72) ? b1 : 14.72;
        if (d < dbp)
            return 20 * log10((40 * M_PI * d * carrierFrequencyHz_) / 3)
                   + a * log10(d) - b + 0.002 * log10(hBuilding_) * d;
        else
            return 20 * log10((40 * M_PI * dbp * carrierFrequencyHz_) / 3)
                   + a * log10(dbp) - b + 0.002 * log10(hBuilding_) * dbp
                   + 40 * log10(d / dbp);
    }
    // NLOS situation
    if (d > 5000) {
        if (tolerateMaxDistViolation_)
            return ATT_MAXDISTVIOLATED;
        else
            throw cRuntimeError("Error: NLOS rural macrocell path loss model is valid for d < 5000 m");
    }

    double att = 161.04 - 7.1 * log10(wStreet_) + 7.5 * log10(hBuilding_)
        - (24.37 - 3.7 * pow(hBuilding_ / hNodeB_, 2)) * log10(hNodeB_)
        + (43.42 - 3.1 * log10(hNodeB_)) * (log10(d) - 3)
        + 20 * log10CarrierFrequencyGHz_
        - (3.2 * (pow(log10(11.75 * hUe_), 2)) - 4.97);
    return att;
}

double LteRealisticChannelModel::getTwoDimDistance(inet::Coord a, inet::Coord b)
{
    a.z = 0.0;
    b.z = 0.0;
    return a.distance(b);
}

double LteRealisticChannelModel::getStdDev(bool dist, MacNodeId nodeId)
{
    switch (scenario_) {
        case URBAN_MICROCELL:
        case INDOOR_HOTSPOT:
            if (losMap_[nodeId])
                return 3.;
            else
                return 4.;
        case URBAN_MACROCELL:
            if (losMap_[nodeId])
                return 4.;
            else
                return 6.;
        case RURAL_MACROCELL:
        case SUBURBAN_MACROCELL:
            if (losMap_[nodeId]) {
                if (dist)
                    return 4.;
                else
                    return 6.;
            }
            else
                return 8.;
        default:
            throw cRuntimeError("Wrong path-loss scenario value %d", scenario_);
    }
    return 0.0;
}

bool LteRealisticChannelModel::computeExtCellInterference(MacNodeId eNbId, MacNodeId nodeId, Coord coord, bool isCqi, GHz carrierFrequency,
        std::vector<double> *interference)
{
    EV << "**** Ext Cell Interference **** " << endl;

    // get external cell list
    ExtCellList list = binder_->getExtCellList(carrierFrequency);

    double dist, // meters
           recvPwr, // watt
           recvPwrDBm, // dBm
           att, // dBm
           angularAtt; // dBm

    //compute distance for each cell
    for (auto& extCell : list) {
        // get external cell position
        Coord c = extCell->getPosition();
        // compute distance between UE and the ext cell
        dist = coord.distance(c);

        EV << "\t distance between UE[" << coord.x << "," << coord.y <<
            "] and extCell[" << c.x << "," << c.y << "] is -> "
           << dist << "\t";

        // compute attenuation according to some path loss model
        att = computeExtCellPathLoss(dist, nodeId);

        //=============== ANGULAR ATTENUATION =================
        if (extCell->getTxDirection() == OMNI) {
            angularAtt = 0;
        }
        else {
            // compute the angle between uePosition and reference axis, considering the eNb as center
            double ueAngle = computeAngle(c, coord);

            // compute the reception angle between ue and eNb
            double recvAngle = fabs(extCell->getTxAngle() - ueAngle);

            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(c, coord);

            // compute attenuation due to sectorial tx
            angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);
        }
        //=============== END ANGULAR ATTENUATION =================

        // TODO do we need to use (- cableLoss_ + antennaGainEnB_) in ext cells too?
        // compute and linearize received power
        recvPwrDBm = extCell->getTxPower() - att - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainUe_;
        recvPwr = dBmToLinear(recvPwrDBm);

        unsigned int numBands = std::min(numBands_, extCell->getNumBands());
        EV << " - shared bands [" << numBands << "]" << endl;

        // add interference in those bands where the ext cell is active
        for (unsigned int i = 0; i < numBands; i++) {
            int occ;
            if (isCqi) { // check slot occupation for this TTI
                occ = extCell->getBandStatus(i);
            }
            else {      // error computation. We need to check the slot occupation of the previous TTI
                occ = extCell->getPrevBandStatus(i);
            }

            // if the ext cell is active, add interference
            if (occ) {
                (*interference)[i] += recvPwr;
            }
        }
    }

    return true;
}

bool LteRealisticChannelModel::computeBackgroundCellInterference(MacNodeId nodeId, inet::Coord bsCoord, inet::Coord ueCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, Direction dir,
        std::vector<double> *interference)
{
    EV << "**** Background Cell Interference **** " << endl;

    // get bg schedulers list
    const auto& list = binder_->getBackgroundSchedulerList(carrierFrequency);

    Coord c;
    double dist, // meters
           txPwr, // dBm
           recvPwr, // watt
           recvPwrDBm, // dBm
           att, // dBm
           angularAtt; // dBm

    //compute distance for each cell
    for (auto& bgScheduler : list) {
        if (dir == DL) {
            // compute interference with respect to the background base station

            // get external cell position
            c = bgScheduler->getPosition();
            // compute distance between UE and the ext cell
            dist = ueCoord.distance(c);

            EV << "\t distance between UE[" << ueCoord.x << "," << ueCoord.y <<
                "] and backgroundCell[" << c.x << "," << c.y << "] is -> "
               << dist << "\t";

            // compute attenuation according to some path loss model
            att = computeExtCellPathLoss(dist, nodeId);

            txPwr = bgScheduler->getTxPower();

            //=============== ANGULAR ATTENUATION =================
            if (bgScheduler->getTxDirection() == OMNI) {
                angularAtt = 0;
            }
            else {
                // compute the angle between uePosition and reference axis, considering the eNB as center
                double ueAngle = computeAngle(c, ueCoord);

                // compute the reception angle between ue and eNB
                double recvAngle = fabs(bgScheduler->getTxAngle() - ueAngle);

                if (recvAngle > 180)
                    recvAngle = 360 - recvAngle;

                double verticalAngle = computeVerticalAngle(c, ueCoord);

                // compute attenuation due to sectorial tx
                angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);
            }
            //=============== END ANGULAR ATTENUATION =================

            // TODO do we need to use (- cableLoss_ + antennaGainEnB_) in ext cells too?
            // compute and linearize received power
            recvPwrDBm = txPwr - att - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainUe_;
            recvPwr = dBmToLinear(recvPwrDBm);
            EV << " recvPwr[" << recvPwr << "]\t";

            unsigned int numBands = std::min(numBands_, bgScheduler->getNumBands());
            EV << " - shared bands [" << numBands << "]\t";
            EV << " - interfering bands[";

            // add interference in those bands where the ext cell is active
            for (unsigned int i = 0; i < numBands; i++) {
                int occ = 0;
                if (isCqi) { // check slot occupation for this TTI
                    occ = bgScheduler->getBandStatus(i, DL);
                }
                else if (!rbmap.empty() && rbmap.at(MACRO).at(i) != 0) {     // error computation. We need to check the slot occupation of the previous TTI (only if the band has been used by the UE)
                    occ = bgScheduler->getPrevBandStatus(i, DL);
                }

                // if the ext cell is active, add interference
                if (occ > 0) {
                    EV << i << ",";
                    (*interference)[i] += recvPwr;
                }
            }
            EV << "]" << endl;
        }
        else { // dir == UL
            // for each RB occupied in the background cell, compute interference with respect to the
            // background UE that is using that RB
            TrafficGeneratorBase *bgUe;

            double antennaGainBgUe = antennaGainUe_;  // TODO get this from the bgUe

            angularAtt = 0;  // we assume OMNI directional UEs

            unsigned int numBands = std::min(numBands_, bgScheduler->getNumBands());
            EV << " - shared bands [" << numBands << "]" << endl;

            // add interference in those bands where a UE in the background cell is active
            for (unsigned int i = 0; i < numBands; i++) {
                int occ = 0;

                if (isCqi) { // check slot occupation for this TTI
                    occ = bgScheduler->getBandStatus(i, UL);
                    if (occ)
                        bgUe = bgScheduler->getBandInterferingUe(i);
                }
                else if (rbmap.at(MACRO).at(i) != 0) {     // error computation. We need to check the slot occupation of the previous TTI (only if the band has been used by the UE)
                    occ = bgScheduler->getPrevBandStatus(i, UL);
                    if (occ)
                        bgUe = bgScheduler->getPrevBandInterferingUe(i);
                }

                // if the ext cell is active, add interference
                if (occ) {
                    txPwr = bgUe->getTxPwr();

                    c = bgUe->getCoord();
                    dist = bsCoord.distance(c);

                    EV << "\t distance between BgBS[" << bsCoord.x << "," << bsCoord.y <<
                        "] and backgroundUE[" << c.x << "," << c.y << "] is -> "
                       << dist << "\t";

                    // compute attenuation according to some path loss model
                    att = computeExtCellPathLoss(dist, nodeId);

                    recvPwrDBm = txPwr - att - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainBgUe;
                    recvPwr = dBmToLinear(recvPwrDBm);

                    (*interference)[i] += recvPwr;
                }
            }
        }
    }

    return true;
}

double LteRealisticChannelModel::computeExtCellPathLoss(double dist, MacNodeId nodeId)
{

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losMap_[nodeId];

    if (!enable_extCell_los_)
        los = false;
    double dbp = 0;
    double attenuation = LteRealisticChannelModel::computePathLoss(dist, dbp, los);

    return attenuation;
}

LteRealisticChannelModel::JakesFadingMap *LteRealisticChannelModel::obtainUeJakesMap(MacNodeId id)
{
    // obtain a reference to UE phy
    LtePhyBase *phy = nullptr;

    for (const auto& ueInfo : binder_->getUeList()) {
        if (ueInfo->id == id) {
            phy = ueInfo->phy;
            break;
        }
    }

    if (phy == nullptr)
        return nullptr;

    // get the associated channel and get a reference to its Jakes Map
    JakesFadingMap *j;
    LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel *>(phy->getChannelModel(carrierFrequency_));
    if (re == nullptr)
        throw cRuntimeError("LteRealisticChannelModel::obtainUeJakesMap - channel model is a null pointer");
    else
        j = re->getJakesMap();

    return j;
}

LteRealisticChannelModel::ShadowFadingMap *LteRealisticChannelModel::obtainShadowingMap(MacNodeId id)
{
    // obtain a reference to UE phy
    LtePhyBase *phy = nullptr;

    for (const auto& ueInfo : binder_->getUeList()) {
        if (ueInfo->id == id) {
            phy = ueInfo->phy;
            break;
        }
    }

    if (phy == nullptr)
        return nullptr;

    // get the associated channel and get a reference to its shadowing Map
    LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel *>(phy->getChannelModel(carrierFrequency_));
    ShadowFadingMap *j = re->getShadowingMap();
    return j;
}

bool LteRealisticChannelModel::computeDownlinkInterference(MacNodeId eNbId, MacNodeId ueId, Coord coord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
        std::vector<double> *interference)
{
    EV << "**** Downlink Interference ****" << endl;

    const auto& enbList = binder_->getEnbList();
    for (auto& enbInfo : enbList) {
        MacNodeId id = enbInfo->id;

        if (id == eNbId)
            continue;

        // initialize eNB data structures
        if (!enbInfo->init) {
            // obtain a reference to eNB phy and obtain tx power
            enbInfo->phy = check_and_cast<LtePhyBase *>(binder_->getPhyByNodeId(id));

            enbInfo->txPwr = enbInfo->phy->getTxPwr();//dBm

            // get tx direction
            enbInfo->txDirection = enbInfo->phy->getTxDirection();

            // get tx angle
            enbInfo->txAngle = enbInfo->phy->getTxAngle();

            //get reference to mac layer
            enbInfo->mac = check_and_cast<LteMacEnb *>(binder_->getMacByNodeId(id));

            enbInfo->init = true;
        }

        LteRealisticChannelModel *interfChanModel = dynamic_cast<LteRealisticChannelModel *>(enbInfo->phy->getChannelModel(carrierFrequency));

        // if the eNB does not use the selected carrier frequency, skip it
        if (interfChanModel == nullptr)
            continue;

        // compute attenuation using data structures within the cell
        double att = interfChanModel->getAttenuation(ueId, UL, coord, isCqi);
        EV << "EnbId [" << id << "] - attenuation [" << att << "]";

        //=============== ANGULAR ATTENUATION =================
        double angularAtt = 0;
        if (enbInfo->txDirection == ANISOTROPIC) {
            //get tx angle
            double txAngle = enbInfo->txAngle;

            // compute the angle between uePosition and reference axis, considering the eNB as center
            double ueAngle = computeAngle(interfChanModel->phy_->getCoord(), coord);

            // compute the reception angle between ue and eNB
            double recvAngle = fabs(txAngle - ueAngle);
            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(interfChanModel->phy_->getCoord(), coord);

            // compute attenuation due to sectorial tx
            angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);

            EV << "angular attenuation [" << angularAtt << "]";
        }
        // else, antenna is omni-directional
        //=============== END ANGULAR ATTENUATION =================

        double txPwr = enbInfo->txPwr - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainUe_;

        unsigned int numBands = std::min(numBands_, interfChanModel->getNumBands());
        EV << " - shared bands [" << numBands << "]" << endl;

        if (isCqi) {// check slot occupation for this TTI
            for (unsigned int i = 0; i < numBands; i++) {
                // compute the number of occupied slot (unnecessary)
                int temp = enbInfo->mac->getDlBandStatus(i);
                if (temp != 0)
                    (*interference)[i] += dBmToLinear(txPwr - att); //(dBm-dB)=dBm

                EV << "\t band " << i << " occupied " << temp << "/pwr[" << txPwr << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
        else { // error computation. We need to check the slot occupation of the previous TTI
            for (unsigned int i = 0; i < numBands; i++) {
                // if we are decoding a data transmission and this RB has not been used, skip it
                // TODO fix for multi-antenna case
                if (!rbmap.empty() && rbmap.at(MACRO).at(i) == 0)
                    continue;

                // compute the number of occupied slot (unnecessary)
                int temp = enbInfo->mac->getDlPrevBandStatus(i);
                if (temp != 0)
                    (*interference)[i] += dBmToLinear(txPwr - att); //(dBm-dB)=dBm

                EV << "\t band " << i << " occupied " << temp << "/pwr[" << txPwr << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
    }

    return true;
}

LteRealisticChannelModel::InterfererInfo LteRealisticChannelModel::describeInterferer(const UeAllocationInfo& allocation)
{
    InterfererInfo info;
    info.nodeId = allocation.nodeId;
    info.cellId = allocation.cellId;
    info.dir = allocation.dir;

    if (allocation.phy != nullptr) {
        LtePhyUe *uePhy = check_and_cast<LtePhyUe *>(allocation.phy);
        info.txPwr = uePhy->getTxPwr(info.dir);
        info.coord = uePhy->getCoord();
    }
    else { // this is a backgroundUe
        TrafficGeneratorBase *trafficGen = check_and_cast<TrafficGeneratorBase *>(allocation.trafficGen);
        info.txPwr = trafficGen->getTxPwr();
        info.coord = trafficGen->getCoord();
    }
    return info;
}

bool LteRealisticChannelModel::computeUplinkInterference(MacNodeId eNbId, MacNodeId senderId, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference)
{
    EV << "**** Uplink Interference for cellId[" << eNbId << "] node[" << senderId << "] ****" << endl;

    const std::vector<std::vector<UeAllocationInfo>> *ulTransmissionMap;
    const std::vector<UeAllocationInfo> *allocatedUes;

    if (isCqi) {// check slot occupation for this TTI
        ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, CURR_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            for (unsigned int i = 0; i < numBands_; i++) {
                // get the set of UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    const InterfererInfo interferer = describeInterferer(ue_it);
                    const MacNodeId ueId = interferer.nodeId;
                    const MacCellId cellId = interferer.cellId;
                    const Direction dir = interferer.dir;
                    const double txPwr = interferer.txPwr;
                    const inet::Coord ueCoord = interferer.coord;

                    // no self-interference
                    if (ueId == senderId)
                        continue;

                    // no interference from UL/D2D connections of the same cell  (no D2D-UL reuse allowed)
                    if (cellId == eNbId)
                        continue;

                    EV << NOW << " LteRealisticChannelModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get rx power and attenuation from this UE
                    double rxPwr = txPwr - cableLoss_ + antennaGainUe_ + antennaGainEnB_;
                    double att = getAttenuation(ueId, UL, ueCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }
    else { // Error computation. We need to check the slot occupation of the previous TTI
        ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, PREV_TTI);
        if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
            // For each band we have to check if the Band in the previous TTI was occupied by the interferingId
            for (unsigned int i = 0; i < numBands_; i++) {
                // if we are decoding a data transmission and this RB has not been used, skip it
                // TODO fix for multi-antenna case
                if (!rbmap.empty() && rbmap.at(MACRO).at(i) == 0)
                    continue;

                // get the set of UEs transmitting on the same band
                allocatedUes = &(ulTransmissionMap->at(i));

                for (auto& ue_it : *allocatedUes) {
                    const InterfererInfo interferer = describeInterferer(ue_it);
                    const MacNodeId ueId = interferer.nodeId;
                    const MacCellId cellId = interferer.cellId;
                    const Direction dir = interferer.dir;
                    const double txPwr = interferer.txPwr;
                    const inet::Coord ueCoord = interferer.coord;

                    // no self-interference
                    if (ueId == senderId)
                        continue;

                    // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                    if (cellId == eNbId)
                        continue;

                    EV << NOW << " LteRealisticChannelModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                    // get tx power and attenuation from this UE
                    double rxPwr = txPwr - cableLoss_ + antennaGainUe_ + antennaGainEnB_;
                    double att = getAttenuation(ueId, UL, ueCoord, false);
                    (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                    EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
                }
            }
        }
    }

    // Debug Output
    EV << NOW << " LteRealisticChannelModel::computeUplinkInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < numBands_; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

bool LteRealisticChannelModel::computeD2DInterference(MacNodeId eNbId, MacNodeId senderId, Coord senderCoord, MacNodeId destId, Coord destCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
        std::vector<double> *interference, Direction dir)
{
    EV << "**** D2D Interference for cellId[" << eNbId << "] node[" << destId << "] ****" << endl;

    // get the reference to the MAC of the eNodeB
    LteMacEnbD2D *macEnb = check_and_cast<LteMacEnbD2D *>(binder_->getMacFromMacNodeId(eNbId));

    const std::vector<std::vector<UeAllocationInfo>> *ulTransmissionMap;
    const std::vector<UeAllocationInfo> *allocatedUes;

    // isCqi: check slot occupation for this TTI; otherwise this is an error
    // computation and we need the previous TTI's occupation instead. That TTI
    // selector is the only difference between the two cases.
    ulTransmissionMap = binder_->getUlTransmissionMap(carrierFrequency, isCqi ? CURR_TTI : PREV_TTI);
    if (ulTransmissionMap != nullptr && !ulTransmissionMap->empty()) {
        // For each band, check whether it was occupied by an interfering UE
        for (unsigned int i = 0; i < numBands_; i++) {
            // get the UEs transmitting on the same band
            allocatedUes = &(ulTransmissionMap->at(i));

            for (auto& ue_it : *allocatedUes) {
                const InterfererInfo interferer = describeInterferer(ue_it);
                const MacNodeId ueId = interferer.nodeId;
                const MacCellId cellId = interferer.cellId;
                const Direction dir = interferer.dir;
                const double txPwr = interferer.txPwr;
                const inet::Coord ueCoord = interferer.coord;

                // no self-interference
                if (ueId == senderId || ueId == destId)
                    continue;

                // no interference from UL connections of the same cell (no D2D-UL reuse allowed)
                if (dir == UL && cellId == eNbId)
                    continue;

                // no interference from D2D connections of the same cell when reuse is disabled (otherwise, computation of CQI is misleading)
                if (cellId == eNbId && (!macEnb->isReuseD2DEnabled() && !macEnb->isReuseD2DMultiEnabled()))
                    continue;

                EV << NOW << " LteRealisticChannelModel::computeD2DInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

                // get tx power and attenuation from this UE
                double rxPwr = txPwr - cableLoss_ + 2 * antennaGainUe_;
                // interferer -> our receiver; the eNB-side maps are used for interferers
                double att = getAttenuation(d2dLink(ueId, ueCoord, destId, destCoord, false));
                (*interference)[i] += dBmToLinear(rxPwr - att);//(dBm-dB)=dBm

                EV << "\t band " << i << "/pwr[" << rxPwr - att << "]-int[" << (*interference)[i] << "]" << endl;
            }
        }
    }

    // Debug Output
    EV << NOW << " LteRealisticChannelModel::computeD2DInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < numBands_; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

} //namespace
