//
//                  Simu5G
//
// Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/phy/channelmodel/SionnaChannelModel.h"

#include "simu5g/common/InitStages.h"
#include "simu5g/stack/phy/LtePhyUe.h"
#include "simu5g/stack/phy/packet/LteAirFrame.h"

namespace simu5g {

Define_Module(SionnaChannelModel);

void SionnaChannelModel::initialize(int stage)
{
    LteRealisticChannelModel::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        // The Sionna path gain spans Tx port -> Rx port (it already includes antenna
        // patterns/beamforming and the fades). Neutralize the analytic terms so they
        // are not double-counted, and disable fading so no RNG is drawn (Sionna is
        // deterministic). The NED defaults already set these, but force them in case
        // an .ini overrides them.
        antennaGainEnB_ = 0.0;
        antennaGainUe_ = 0.0;
        antennaGainMicro_ = 0.0;
        cableLoss_ = 0.0;
        shadowing_ = false;
        fading_ = false;

        // Background/external cells are NOT in the ray-traced scene, so their
        // interference could only come from the analytic model - inconsistent with the
        // Sionna desired signal, and further distorted by the antenna/cable/angular
        // terms zeroed above (which the analytic background path needs). Disable them
        // for v1 (the scene IS the geometry); in-scene multi-cell interference
        // (computeDownlink/UplinkInterference, sourced from the table) is unaffected.
        if (enableBackgroundCellInterference_ || enableExtCellInterference_)
            EV_WARN << "SionnaChannelModel: background/external-cell interference is not "
                       "supported with the ray-traced channel (Plan A v1) - disabling it.\n";
        enableBackgroundCellInterference_ = false;
        enableExtCellInterference_ = false;
    }
    else if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        sionnaManager_.reference(this, "sionnaManagerModule", true);
    }
}

void SionnaChannelModel::setPhy(LtePhyBase *phy)
{
    LteChannelModel::setPhy(phy); // sets phy_
    // announce this node to the manager so the table covers it (the phy/position
    // is valid here, unlike the Binder lists this early in initialization)
    if (sionnaManager_)
        sionnaManager_->registerNode(phy);
}

double SionnaChannelModel::meanPathGainDb(const inet::Coord& a, const inet::Coord& b)
{
    const std::vector<double> *pg = sionnaManager_->getPathGainVector(a, b, carrierFrequency_);
    if (pg == nullptr)
        throw cRuntimeError("SionnaChannelModel: no Sionna path gain between (%g,%g,%g) and "
                "(%g,%g,%g) on carrier %g GHz (Plan A v1 is static - a node may have moved out of the ray-traced table)", a.x, a.y, a.z, b.x, b.y, b.z, carrierFrequencyGHz_);
    double sum = 0.0;
    for (double g : *pg)
        sum += g;
    return sum / pg->size();
}

std::vector<double> SionnaChannelModel::desiredRecvPowerPerBand(double txPower,
        const inet::Coord& thisPos, const inet::Coord& otherPos)
{
    const std::vector<double> *pg = sionnaManager_->getPathGainVector(thisPos, otherPos, carrierFrequency_);
    if (pg == nullptr)
        throw cRuntimeError("SionnaChannelModel: no Sionna path gain between (%g,%g,%g) and "
                "(%g,%g,%g) on carrier %g GHz", thisPos.x, thisPos.y, thisPos.z,
                otherPos.x, otherPos.y, otherPos.z, carrierFrequencyGHz_); // static-scenario assumption

    std::vector<double> recv(numBands_, 0.0);
    for (unsigned int i = 0; i < numBands_; i++) {
        // wideband tables carry a single value; perRb tables are indexed by band
        double g = (pg->size() == 1) ? (*pg)[0] : (i < pg->size() ? (*pg)[i] : pg->back());
        recv[i] = txPower + g; // (dBm + dB) = dBm
    }
    return recv;
}

double SionnaChannelModel::getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl)
{
    // endpoints are identified by position: this node (phy_) and `coord` (the other).
    // Used for the desired link's wideband level and, via the inherited interference
    // routines, for interferer attenuation. Attenuation [dB] = -mean path gain [dB].
    return -meanPathGainDb(phy_->getCoord(), coord);
}

double SionnaChannelModel::getAttenuation_D2D(MacNodeId nodeId, Direction dir, inet::Coord coord,
        MacNodeId node2_Id, inet::Coord coord_2, bool cqiDl)
{
    return -meanPathGainDb(coord, coord_2);
}

double SionnaChannelModel::computeShadowing(double sqrDistance, MacNodeId nodeId, double speed, bool cqiDl)
{
    return 0.0; // the ray-traced path gain already encodes large-scale shadowing
}

double SionnaChannelModel::computeAngularAttenuation(double hAngle, double vAngle)
{
    return 0.0; // the ray-traced path gain already encodes antenna patterns/beamforming
}

std::vector<double> SionnaChannelModel::getSINR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    double txPower = lteInfo->getTxPower(); // dBm
    RbMap rbmap = lteInfo->getGrantedBlocks();
    inet::Coord coord = lteInfo->getCoord(); // the other endpoint (sender)
    Direction dir = (Direction)lteInfo->getDirection();
    bool feedback = (lteInfo->getFrameType() == FEEDBACKPKT);

    double noiseFigure = 0.0;
    MacNodeId ueId = NODEID_NONE, eNbId = NODEID_NONE;
    inet::Coord ueCoord, enbCoord;

    if (dir == DL && !feedback) {
        noiseFigure = ueNoiseFigure_;
        ueId = lteInfo->getDestId();
        eNbId = lteInfo->getSourceId();
        ueCoord = phy_->getCoord();
        enbCoord = coord;
    }
    else { // UL / DL-CQI (feedback) / UL error
        ueId = lteInfo->getSourceId();
        eNbId = lteInfo->getDestId();
        noiseFigure = (dir == DL) ? ueNoiseFigure_ : bsNoiseFigure_;
        ueCoord = coord;
        enbCoord = phy_->getCoord();
    }

    // desired received power per band, straight from the Sionna table
    std::vector<double> snrVector = desiredRecvPowerPerBand(txPower, phy_->getCoord(), coord);

    // interference + noise: reuse the inherited aggregation (it sources interferer
    // attenuation through the overridden getAttenuation, i.e. from the Sionna table)
    std::vector<double> multiCellInterference(numBands_, 0.0);
    std::vector<double> bgCellInterference(numBands_, 0.0);
    std::vector<double> extCellInterference(numBands_, 0.0);

    if (enableDownlinkInterference_ && dir == DL && lteInfo->getFrameType() != HANDOVERPKT)
        computeDownlinkInterference(eNbId, ueId, ueCoord, feedback, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);
    else if (enableUplinkInterference_ && dir == UL)
        computeUplinkInterference(eNbId, ueId, feedback, lteInfo->getCarrierFrequency(), rbmap, &multiCellInterference);

    if (enableBackgroundCellInterference_)
        computeBackgroundCellInterference(ueId, enbCoord, ueCoord, feedback, lteInfo->getCarrierFrequency(), rbmap, dir, &bgCellInterference);

    if (enableExtCellInterference_ && dir == DL)
        computeExtCellInterference(eNbId, ueId, ueCoord, feedback, lteInfo->getCarrierFrequency(), &extCellInterference);

    double totN = dBmToLinear(thermalNoise_ + noiseFigure);
    double sumSnr = 0.0;
    int usedRBs = 0;
    for (unsigned int i = 0; i < numBands_; i++) {
        if (lteInfo->getFrameType() == DATAPKT && rbmap[MACRO][i] == 0)
            continue;
        double den = linearToDBm(bgCellInterference[i] + extCellInterference[i] + totN + multiCellInterference[i]);
        snrVector[i] -= den;
        sumSnr += snrVector[i];
        ++usedRBs;
    }

    // emit measured-SINR statistics on feedback, mirroring LteRealisticChannelModel
    if (collectSinrStatistics_ && feedback && usedRBs > 0) {
        LteChannelModel *ueChannelModel = check_and_cast<LtePhyUe *>(binder_->getPhyByNodeId(ueId))->getChannelModel(lteInfo->getCarrierFrequency());
        if (dir == DL)
            ueChannelModel->emit(measuredSinrDlSignal_, sumSnr / usedRBs);
        else
            ueChannelModel->emit(measuredSinrUlSignal_, sumSnr / usedRBs);
    }

    return snrVector;
}

std::vector<double> SionnaChannelModel::getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    double txPower = lteInfo->getTxPower();
    inet::Coord coord = lteInfo->getCoord();
    // received useful signal per band (no interference), from the Sionna table
    return desiredRecvPowerPerBand(txPower, phy_->getCoord(), coord);
}

double SionnaChannelModel::getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos,
        Direction dir, bool losStatus, MacNodeId bsId)
{
    const std::vector<double> *pg = sionnaManager_->getPathGainVector(txPos, rxPos, carrierFrequency_);
    if (pg == nullptr)
        // background UEs may live outside the ray-traced scene; fall back to the analytic model
        return LteRealisticChannelModel::getReceivedPower_bgUe(txPower, txPos, rxPos, dir, losStatus, bsId);

    double sum = 0.0;
    for (double g : *pg)
        sum += g;
    return txPower + sum / pg->size();
}

} //namespace
