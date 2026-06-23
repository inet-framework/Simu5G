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

#ifndef STACK_PHY_CHANNELMODEL_COMPARECHANNELMODEL_H_
#define STACK_PHY_CHANNELMODEL_COMPARECHANNELMODEL_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"

namespace simu5g {

using namespace omnetpp;

//
// Validation decorator over two inner channel models. See CompareChannelModel.ned.
//
class CompareChannelModel : public LteRealisticChannelModel
{
  protected:
    inet::ModuleRefByPar<LteChannelModel> reference_;
    inet::ModuleRefByPar<LteChannelModel> candidate_;
    bool primaryIsReference_ = true;

    simsignal_t attenuationDeltaSignal_;
    simsignal_t rsrpDeltaSignal_;
    simsignal_t sinrDeltaSignal_;

    LteChannelModel *primary() { return primaryIsReference_ ? reference_.get() : candidate_.get(); }
    static double mean(const std::vector<double>& v);

  public:
    void initialize(int stage) override;
    void setPhy(LtePhyBase *phy) override;

    // compared methods: forward to both, emit delta, return primary's result
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl) override;
    std::vector<double> getSINR(LteAirFrame *frame, UserControlInfo *lteInfo) override;
    std::vector<double> getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    // forwarded to both, primary returned (no dedicated delta signal)
    std::vector<double> getSIR(LteAirFrame *frame, UserControlInfo *lteInfo) override;
    std::vector<double> getSINR_bgUe(LteAirFrame *frame, UserControlInfo *lteInfo) override;
    double getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus, MacNodeId bsId) override;
    std::vector<double> getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId peerUeId, inet::Coord peerUeCoord, MacNodeId enbId) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector) override;
    double computePathLoss(double distance, double dbp, bool los) override;

    // reception draws RNG: forward to the primary only so the run stays RNG-neutral
    bool isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo) override;
    bool isReceptionSuccessful_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, const std::vector<double>& rsrpVector) override;

    bool isUplinkInterferenceEnabled() override { return primary()->isUplinkInterferenceEnabled(); }
    bool isD2DInterferenceEnabled() override { return primary()->isD2DInterferenceEnabled(); }

    // The built-in model's cross-model shadowing/fading coupling fetches the peer's
    // map via dynamic_cast<LteRealisticChannelModel*> + these accessors. Forward them
    // to the reference inner model so a built-in peer and this decorator share one
    // consistent map (otherwise the eNB writes this module's unused map while the
    // reference inner uses its own -> divergent shadowing -> wrong/mismatched).
    ShadowFadingMap *getShadowingMap() override
    {
        if (reference_) {
            auto *r = dynamic_cast<LteRealisticChannelModel *>(reference_.get());
            if (r != nullptr)
                return r->getShadowingMap();
        }
        return LteRealisticChannelModel::getShadowingMap();
    }

    JakesFadingMap *getJakesMap() override
    {
        if (reference_) {
            auto *r = dynamic_cast<LteRealisticChannelModel *>(reference_.get());
            if (r != nullptr)
                return r->getJakesMap();
        }
        return LteRealisticChannelModel::getJakesMap();
    }
};

} //namespace

#endif
