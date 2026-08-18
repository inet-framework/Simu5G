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

#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"

#include <fstream>
#include "simu5g/common/cellInfo/CellInfo.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/mac/amc/UserTxParams.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/nodes/ExtCell.h"
#include "simu5g/background/cell/BackgroundScheduler.h"
#include "simu5g/stack/phy/PhyUe.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/phy/channelmodel/RadioMedium.h"
#include "simu5g/stack/phy/channelmodel/Tr36814PathLossModel.h"
#include "simu5g/stack/phy/channelmodel/Tr36873PathLossModel.h"
#include "simu5g/stack/phy/channelmodel/Tr38901PathLossModel.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;
Define_Module(StochasticChannelModel);

simsignal_t StochasticChannelModel::rcvdSinrDlSignal_ = registerSignal("rcvdSinrDl");
simsignal_t StochasticChannelModel::rcvdSinrUlSignal_ = registerSignal("rcvdSinrUl");

simsignal_t StochasticChannelModel::measuredSinrDlSignal_ = registerSignal("measuredSinrDl");
simsignal_t StochasticChannelModel::measuredSinrUlSignal_ = registerSignal("measuredSinrUl");

StochasticChannelModel::~StochasticChannelModel()
{
    // the medium may already be gone by the time this endpoint is torn down
    cModule *medium = getSimulation()->getModule(mediumModuleId_);
    if (medium != nullptr)
        check_and_cast<RadioMedium *>(medium)->removeRadio(this);

    delete extCellPathLoss_;
}

void StochasticChannelModel::initialize(int stage)
{
    ChannelModelBase::initialize(stage);
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
        delayRMS_ = par("delayRms");

        enable_extCell_los_ = par("enableExtCellLos");

        collectSinrStatistics_ = par("collectSinrStatistics");
    }
    else if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        // carrierFrequencyHz_/GHz_/log10CarrierFrequencyGHz_ have just been set
        // by ChannelModelBase::initialize() above, in this same stage
        extCellPathLoss_ = new Tr36814PathLossModel();
        extCellPathLoss_->initialize(this, scenario_, hNodeB_, hUe_, hBuilding_, wStreet_,
                carrierFrequencyHz_, carrierFrequencyGHz_, log10CarrierFrequencyGHz_,
                tolerateMaxDistViolation_);
    }
    else if (stage == INITSTAGE_SIMU5G_NODE_RELATIONSHIPS) {
        // phy_ is set at INITSTAGE_SIMU5G_REGISTRATIONS2, so both phy_ and
        // this endpoint's identity are valid by the time it registers here
        medium_.reference(this, "radioMediumModule", true);
        mediumModuleId_ = medium_->getId();
        medium_->addRadio(this);
        // S8: this endpoint's stochastic state now lives in the medium;
        // cache the reference so the state accessors do not look it up
        // (by identity) on every call
        state_ = &medium_->stateOf(this);

        // clear jakes fading map structure; must run here rather than at
        // INITSTAGE_LOCAL, since state_ does not exist yet at that stage
        // (a freshly registered radio's state is already empty by
        // construction, so this is a no-op either way)
        jakesState().clear();
    }
}

RadioLink StochasticChannelModel::cellularLink(MacNodeId ueId, Direction dir, Coord coord, bool cqiDl)
{
    // The local module is one endpoint and 'coord' the other; 'dir' says which
    // of the two is the UE. The UE is the node whose channel state we track.
    RadioLink link;
    link.dir = dir;
    // A cellular link: the degenerate key {ueId, ueId} reproduces the historical
    // node-keyed behavior exactly, since a UE has one such link per instance.
    link.stateKey = LinkKey(ueId);
    link.stateNodeId = ueId;
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

RadioLink StochasticChannelModel::linkFor(UserControlInfo *lteInfo)
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
    link.stateKey = LinkKey(ueId);
    link.stateNodeId = ueId;
    link.stateCoord = ueCoord;

    // the cell this link belongs to, for the interference computation
    link.cellId = eNbId;

    return link;
}

double StochasticChannelModel::getAttenuation(const RadioLink& link)
{
    return medium_->getAttenuation(this, link);
}

double StochasticChannelModel::computeShadowing(double d3D, double d2D, const LinkKey& key, MacNodeId ownerId, double speed, bool cqiDl)
{
    return medium_->computeShadowing(this, d3D, d2D, key, ownerId, speed, cqiDl);
}

void StochasticChannelModel::updatePositionHistory(const MacNodeId nodeId,
        const Coord coord)
{
    // createIfMissing=true: a freshly vivified (empty) queue makes
    // "!history->empty()" false, exactly like the old find()==end() check
    std::queue<Position> *history = positionHistory(nodeId, true);

    if (!history->empty() && history->back().first == NOW)
        // position already updated for this TTI.
        return;

    // FIXME: possible memory leak
    history->push(Position(NOW, coord));

    if (history->size() > 2) // if we have more than a past and a current element
        // drop the oldest one
        history->pop();
}

void StochasticChannelModel::updateCorrelationDistance(const LinkKey& nodeId, const inet::Coord coord) {

    bool existed = false;
    Position& point = correlationPoint(nodeId, &existed);

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

double StochasticChannelModel::computeCorrelationDistance(const LinkKey& key, const inet::Coord coord)
{
    return medium_->computeCorrelationDistance(this, key, coord);
}

double StochasticChannelModel::computeSpeed(const MacNodeId nodeId, const Coord coord)
{
    return medium_->computeSpeed(this, nodeId, coord);
}

double StochasticChannelModel::computeAngle(Coord center, Coord point) {
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

double StochasticChannelModel::computeVerticalAngle(Coord center, Coord point)
{
    double threeDimDistance = center.distance(point);
    double twoDimDistance = getTwoDimDistance(center, point);
    double arccos = acos(twoDimDistance / threeDimDistance) * 180.0 / M_PI;
    return 90 + arccos;
}

double StochasticChannelModel::computeAngularAttenuation(double hAngle, double vAngle) {
    return medium_->pathLossFor(getNodeId(), getCarrierFrequency()).computeAngularAttenuation(hAngle, vAngle);
}

std::vector<double> StochasticChannelModel::getSINR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return medium_->getSINR(this, frame, lteInfo);
}

std::vector<double> StochasticChannelModel::getSINR(const RadioLink& link, UserControlInfo *lteInfo, std::vector<double> snrVector)
{
    return medium_->getSINR(this, link, lteInfo, snrVector);
}

void StochasticChannelModel::computeInterferencePlusNoise(const RadioLink& link, UserControlInfo *lteInfo,
        RbMap& rbmap, double totN, std::vector<double>& den)
{
    medium_->computeInterferencePlusNoise(this, link, lteInfo, rbmap, totN, den);
}

std::vector<double> StochasticChannelModel::getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return medium_->getRSRP(this, frame, lteInfo);
}

std::vector<double> StochasticChannelModel::getRSRP(const RadioLink& link, double txPower)
{
    return medium_->getRSRP(this, link, txPower);
}

std::vector<double> StochasticChannelModel::getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return medium_->getSINR_bgUe(this, frame, lteInfo);
}

double StochasticChannelModel::getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId)
{
    return medium_->getReceivedPower_bgUe(this, txPower, txPos, rxPos, dir, losStatus, bsId);
}

std::vector<double> StochasticChannelModel::getSIR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return medium_->getSIR(this, frame, lteInfo);
}

double StochasticChannelModel::rayleighFading(MacNodeId id, unsigned int band)
{
    return medium_->rayleighFading(this, id, band);
}

double StochasticChannelModel::jakesFading(const LinkKey& key, MacNodeId ownerId, double speed,
        unsigned int band, bool cqiDl, bool isBgUe)
{
    return medium_->jakesFading(this, key, ownerId, speed, band, cqiDl, isBgUe);
}

bool StochasticChannelModel::isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector)
{
    return medium_->isReceptionSuccessful(this, frame, lteInfo, rsrpVector);
}

void StochasticChannelModel::emitRcvdSinr(Direction dir, MacNodeId ueId, GHz carrierFrequency, double sinr)
{
    if (dir == DL) { // we are on the UE
        emit(rcvdSinrDlSignal_, sinr);
        return;
    }

    // we are on the BS, so we need to retrieve the channel model of the sender
    // XXX I know, there might be a faster way...
    ChannelModelBase *ueChannelModel = check_and_cast<PhyUe *>(binder_->getPhyByNodeId(ueId))->getChannelModel(carrierFrequency);
    ueChannelModel->emit(rcvdSinrUlSignal_, sinr);
}

void StochasticChannelModel::computeLosProbability(double d3D, double d2D, const LinkKey& key)
{
    medium_->computeLosProbability(this, d3D, d2D, key);
}

double StochasticChannelModel::computePathLoss(double distance, double dbp, bool los)
{
    return medium_->computePathLoss(this, distance, dbp, los);
}

double StochasticChannelModel::getTwoDimDistance(inet::Coord a, inet::Coord b)
{
    a.z = 0.0;
    b.z = 0.0;
    return a.distance(b);
}

bool StochasticChannelModel::computeExtCellInterference(MacNodeId eNbId, MacNodeId nodeId, Coord coord, bool isCqi, GHz carrierFrequency,
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
        att = computeExtCellPathLoss(dist, LinkKey(nodeId));

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

bool StochasticChannelModel::computeBackgroundCellInterference(MacNodeId nodeId, inet::Coord bsCoord, inet::Coord ueCoord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, Direction dir,
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
            att = computeExtCellPathLoss(dist, LinkKey(nodeId));

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
                    att = computeExtCellPathLoss(dist, LinkKey(nodeId));

                    recvPwrDBm = txPwr - att - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainBgUe;
                    recvPwr = dBmToLinear(recvPwrDBm);

                    (*interference)[i] += recvPwr;
                }
            }
        }
    }

    return true;
}

double StochasticChannelModel::computeExtCellPathLoss(double dist, const LinkKey& nodeId)
{

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = losState(nodeId);

    if (!enable_extCell_los_)
        los = false;

    // always the TR 36.814 formulas, whatever study the model itself uses
    double attenuation = extCellPathLoss_->computePathLoss(dist, dist, los, O2iState{inside_building_, inside_distance_});

    return attenuation;
}

JakesFadingMap *StochasticChannelModel::obtainUeJakesMap(MacNodeId id)
{
    // obtain a reference to UE phy
    PhyBase *phy = nullptr;

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
    StochasticChannelModel *re = dynamic_cast<StochasticChannelModel *>(phy->getChannelModel(carrierFrequency_));
    if (re == nullptr)
        throw cRuntimeError("StochasticChannelModel::obtainUeJakesMap - channel model is a null pointer");
    else
        j = re->getJakesMap();

    return j;
}

ShadowFadingMap *StochasticChannelModel::obtainShadowingMap(MacNodeId id)
{
    // obtain a reference to UE phy
    PhyBase *phy = nullptr;

    for (const auto& ueInfo : binder_->getUeList()) {
        if (ueInfo->id == id) {
            phy = ueInfo->phy;
            break;
        }
    }

    if (phy == nullptr)
        return nullptr;

    // get the associated channel and get a reference to its shadowing Map
    StochasticChannelModel *re = dynamic_cast<StochasticChannelModel *>(phy->getChannelModel(carrierFrequency_));
    ShadowFadingMap *j = re->getShadowingMap();
    return j;
}

bool StochasticChannelModel::computeDownlinkInterference(MacNodeId eNbId, MacNodeId ueId, Coord coord, bool isCqi, GHz carrierFrequency, const RbMap& rbmap,
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
            enbInfo->phy = check_and_cast<PhyBase *>(binder_->getPhyByNodeId(id));

            enbInfo->txPwr = enbInfo->phy->getTxPwr();//dBm

            // get tx direction
            enbInfo->txDirection = enbInfo->phy->getTxDirection();

            // get tx angle
            enbInfo->txAngle = enbInfo->phy->getTxAngle();

            //get reference to mac layer
            enbInfo->mac = check_and_cast<LteMacEnb *>(binder_->getMacByNodeId(id));

            enbInfo->init = true;
        }

        StochasticChannelModel *interfChanModel = dynamic_cast<StochasticChannelModel *>(enbInfo->phy->getChannelModel(carrierFrequency));

        // if the eNB does not use the selected carrier frequency, skip it
        if (interfChanModel == nullptr)
            continue;

        // compute attenuation using data structures within the cell
        double att = interfChanModel->getAttenuation(ueId, UL, coord, isCqi);
        EV << "EnbId [" << id << "] - attenuation [" << att << "]";

        //=============== ANGULAR ATTENUATION =================
        double angularAtt = 0;
        if (medium_->txDirectionOf(id, carrierFrequency) == ANISOTROPIC) {
            //get tx angle
            double txAngle = medium_->txAngleOf(id, carrierFrequency);

            // compute the angle between uePosition and reference axis, considering the eNB as center
            double ueAngle = computeAngle(medium_->coordOf(id, carrierFrequency), coord);

            // compute the reception angle between ue and eNB
            double recvAngle = fabs(txAngle - ueAngle);
            if (recvAngle > 180)
                recvAngle = 360 - recvAngle;

            double verticalAngle = computeVerticalAngle(medium_->coordOf(id, carrierFrequency), coord);

            // compute attenuation due to sectorial tx
            angularAtt = computeAngularAttenuation(recvAngle, verticalAngle);

            EV << "angular attenuation [" << angularAtt << "]";
        }
        // else, antenna is omni-directional
        //=============== END ANGULAR ATTENUATION =================

        double txPwr = medium_->txPowerOf(id, carrierFrequency) - angularAtt - cableLoss_ + antennaGainEnB_ + antennaGainUe_;

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

StochasticChannelModel::InterfererInfo StochasticChannelModel::describeInterferer(const UeAllocationInfo& allocation, RadioMedium *medium, GHz carrierFrequency)
{
    InterfererInfo info;
    info.nodeId = allocation.nodeId;
    info.cellId = allocation.cellId;
    info.dir = allocation.dir;

    if (allocation.phy != nullptr) {
        // a real UE is a registered radio; read its physical facts from the medium
        info.txPwr = medium->txPowerOf(info.nodeId, carrierFrequency, info.dir);
        info.coord = medium->coordOf(info.nodeId, carrierFrequency);
    }
    else { // this is a backgroundUe -- not a registered radio (S12 adds phantom registration)
        TrafficGeneratorBase *trafficGen = check_and_cast<TrafficGeneratorBase *>(allocation.trafficGen);
        info.txPwr = trafficGen->getTxPwr();
        info.coord = trafficGen->getCoord();
    }
    return info;
}

bool StochasticChannelModel::computeUplinkInterference(MacNodeId eNbId, MacNodeId senderId, bool isCqi, GHz carrierFrequency, const RbMap& rbmap, std::vector<double> *interference)
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
                    const InterfererInfo interferer = describeInterferer(ue_it, medium_.get(), carrierFrequency);
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

                    EV << NOW << " StochasticChannelModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

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
                    const InterfererInfo interferer = describeInterferer(ue_it, medium_.get(), carrierFrequency);
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

                    EV << NOW << " StochasticChannelModel::computeUplinkInterference - Interference from UE: " << ueId << "(dir " << dirToA(dir) << ") on band[" << i << "]" << endl;

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
    EV << NOW << " StochasticChannelModel::computeUplinkInterference - Final Band Interference Status: " << endl;
    for (unsigned int i = 0; i < numBands_; i++)
        EV << "\t band " << i << " int[" << (*interference)[i] << "]" << endl;

    return true;
}

} //namespace
