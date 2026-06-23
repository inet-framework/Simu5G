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

#include "simu5g/stack/phy/channelmodel/CompareChannelModel.h"

#include "simu5g/common/InitStages.h"

namespace simu5g {

Define_Module(CompareChannelModel);

double CompareChannelModel::mean(const std::vector<double>& v)
{
    if (v.empty())
        return 0.0;
    double s = 0.0;
    for (double x : v)
        s += x;
    return s / v.size();
}

void CompareChannelModel::initialize(int stage)
{
    LteChannelModel::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        primaryIsReference_ = (std::string(par("primary").stringValue()) != "candidate");
        attenuationDeltaSignal_ = registerSignal("attenuationDelta");
        rsrpDeltaSignal_ = registerSignal("rsrpDelta");
        sinrDeltaSignal_ = registerSignal("sinrDelta");
    }
    else if (stage == INITSTAGE_SIMU5G_POSTLOCAL) {
        reference_.reference(this, "referenceModule", true);
        candidate_.reference(this, "candidateModule", true);
        // the inner models are not the PHY's channelModelModule, so propagate the phy
        if (phy_ != nullptr) {
            reference_->setPhy(phy_);
            candidate_->setPhy(phy_);
        }
    }
}

void CompareChannelModel::setPhy(LtePhyBase *phy)
{
    LteChannelModel::setPhy(phy);
    if (reference_ && candidate_) {
        reference_->setPhy(phy);
        candidate_->setPhy(phy);
    }
}

// ---- compared methods: forward to both, emit delta, return primary ----------

double CompareChannelModel::getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl)
{
    double ref = reference_->getAttenuation(nodeId, dir, coord, cqiDl);
    double cand = candidate_->getAttenuation(nodeId, dir, coord, cqiDl);
    emit(attenuationDeltaSignal_, cand - ref);
    return primaryIsReference_ ? ref : cand;
}

std::vector<double> CompareChannelModel::getSINR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    std::vector<double> ref = reference_->getSINR(frame, lteInfo);
    std::vector<double> cand = candidate_->getSINR(frame, lteInfo);
    emit(sinrDeltaSignal_, mean(cand) - mean(ref));
    return primaryIsReference_ ? ref : cand;
}

std::vector<double> CompareChannelModel::getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    std::vector<double> ref = reference_->getRSRP(frame, lteInfo);
    std::vector<double> cand = candidate_->getRSRP(frame, lteInfo);
    emit(rsrpDeltaSignal_, mean(cand) - mean(ref));
    return primaryIsReference_ ? ref : cand;
}

// ---- forwarded to the primary (reception draws RNG; keep the run neutral) ----

bool CompareChannelModel::isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    // The simulation drives reception through isReceptionSuccessful (each model
    // computes its own getSINR internally), so this is where we probe both models
    // on identical inputs and record the deltas. With the built-in model's fading
    // and shadowing disabled (Plan A §7 large-scale mode) these probes draw no RNG,
    // so the run stays neutral and only the primary performs the reception draw.
    std::vector<double> refSinr = reference_->getSINR(frame, lteInfo);
    std::vector<double> candSinr = candidate_->getSINR(frame, lteInfo);
    emit(sinrDeltaSignal_, mean(candSinr) - mean(refSinr));

    std::vector<double> refRsrp = reference_->getRSRP(frame, lteInfo);
    std::vector<double> candRsrp = candidate_->getRSRP(frame, lteInfo);
    emit(rsrpDeltaSignal_, mean(candRsrp) - mean(refRsrp));

    return primary()->isReceptionSuccessful(frame, lteInfo);
}

bool CompareChannelModel::isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector)
{
    return primary()->isReceptionSuccessful_D2D(frame, lteInfo, rsrpVector);
}

// ---- forwarded to the primary (no dedicated delta in v1) ---------------------

std::vector<double> CompareChannelModel::getSIR(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return primary()->getSIR(frame, lteInfo);
}

std::vector<double> CompareChannelModel::getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    return primary()->getSINR_bgUe(frame, lteInfo);
}

double CompareChannelModel::getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId)
{
    return primary()->getReceivedPower_bgUe(txPower, txPos, rxPos, dir, losStatus, bsId);
}

std::vector<double> CompareChannelModel::getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord)
{
    return primary()->getRSRP_D2D(frame, lteInfo_1, destId, destCoord);
}

std::vector<double> CompareChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId peerUeId, inet::Coord peerUeCoord, MacNodeId enbId)
{
    return primary()->getSINR_D2D(frame, lteInfo, peerUeId, peerUeCoord, enbId);
}

std::vector<double> CompareChannelModel::getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector)
{
    return primary()->getSINR_D2D(frame, lteInfo_1, destId, destCoord, enbId, rsrpVector);
}

double CompareChannelModel::computePathLoss(double distance, double dbp, bool los)
{
    return primary()->computePathLoss(distance, dbp, los);
}

} //namespace
