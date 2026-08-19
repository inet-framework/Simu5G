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

#include "simu5g/stack/phy/channelmodel/RadioMedium.h"

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
}

void StochasticChannelModel::initialize(int stage)
{
    ChannelModelBase::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        inside_building_ = par("insideBuilding");
        if (inside_building_)
            inside_distance_ = uniform(0.0, 25.0);

        antennaGainUe_ = par("antennaGainUe");
        antennaGainEnB_ = par("antennGainEnB");
        cableLoss_ = par("cableLoss");
        ueNoiseFigure_ = par("ueNoiseFigure");
        bsNoiseFigure_ = par("bsNoiseFigure");

        // fadingType has no medium-side validation the way pathLossType does
        // (createPathLossModel's else-throw) -- kept here so a misconfigured
        // NED value still fails fast at init instead of silently drawing no
        // fading at all (RadioMedium's fadingType dispatch has no else branch)
        std::string fType = par("fadingType");
        if (fType != "JAKES" && fType != "RAYLEIGH")
            throw cRuntimeError("Unrecognized value in 'fadingType' parameter: \"%s\"", fType.c_str());

        enableUplinkInterference_ = par("uplinkInterference");

        enable_extCell_los_ = par("enableExtCellLos");

        collectSinrStatistics_ = par("collectSinrStatistics");
    }
    else if (stage == INITSTAGE_SIMU5G_NODE_RELATIONSHIPS) {
        // phy_ is set at INITSTAGE_SIMU5G_REGISTRATIONS2, so both phy_ and
        // this endpoint's identity are valid by the time it registers here
        medium_.reference(this, "radioMediumModule", true);
        mediumModuleId_ = medium_->getId();
        medium_->addRadio(this);
    }
}

RadioLink StochasticChannelModel::cellularLink(MacNodeId ueId, Direction dir, Coord coord)
{
    // The local module is one endpoint and 'coord' the other; 'dir' says which
    // of the two is the UE. The UE is the node whose channel state we track.
    RadioLink link;
    link.dir = dir;
    // A cellular link: the degenerate key {ueId, ueId} reproduces the historical
    // node-keyed behavior exactly, since a UE has one such link per instance.
    link.stateKey = LinkKey(ueId);
    link.stateNodeId = ueId;

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

bool StochasticChannelModel::isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector)
{
    return medium_->isReceptionSuccessful(this, frame, lteInfo, rsrpVector);
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

double StochasticChannelModel::computeExtCellPathLoss(double dist, const LinkKey& key)
{

    //compute attenuation based on selected scenario and based on LOS or NLOS
    bool los = medium_->losStateFor(this, key);

    if (!enable_extCell_los_)
        los = false;

    // always the TR 36.814 formulas, whatever study the model itself uses
    double attenuation = medium_->extCellPathLossFor(getNodeId(), getCarrierFrequency())
            .computePathLoss(dist, dist, los, O2iState{inside_building_, inside_distance_});

    return attenuation;
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
    else { // a background UE is a registered phantom radio
        BgUeKey key{allocation.cellId, carrierFrequency, allocation.nodeId};
        info.txPwr = medium->txPowerOf(key);
        info.coord = medium->coordOf(key);
    }
    return info;
}

} //namespace
