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

#ifndef STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_
#define STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_

#include "simu5g/stack/d2d/phy/channelmodel/ID2dChannelModel.h"
#include "simu5g/stack/phy/channelmodel/StochasticChannelModel.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

/**
 * Marker for D2D-capable NICs, plus the ID2dChannelModel entry points D2D-aware
 * PHY code calls directly (getRSRP_D2D/getSINR_D2D).
 *
 * D2D channel math (attenuation, RSRP/SINR, interference, reception decision)
 * lives as D2D-aware branches in RadioMedium
 * (d2dLink, getReceptionSinr, emitRcvdSinr, computeInterferencePlusNoise) and
 * in its interference submodule (computeD2DInterference), reached from a
 * registered radio's RadioDescriptor::d2dEndpoint -- the medium's marker for
 * "this endpoint is D2D-capable", set once at registration by downcasting to
 * this class. What is left here is the marker itself (being this class),
 * getRSRP_D2D/getSINR_D2D (called from D2dUePhyHelper.cc and PhyEnbD2D.cc),
 * and the two small facts only a D2D-capable
 * endpoint carries: whether D2D interference is enabled, and the signal a
 * D2D reception is reported under.
 *
 * The rcvdSinrD2D signal is owned and interned here, not in the core channel model.
 */
class D2dChannelModel : public StochasticChannelModel, public ID2dChannelModel
{
  protected:
    // enable/disable the interference computation for D2D connections
    bool enableD2DInterference_ = false;

    // Interned in initialize() rather than registered by a static initializer, so that
    // linking the D2D package in cannot shift the signal ids the core assigns. See the
    // "Signals" note in D2dUeMacBase.h.
    simsignal_t rcvdSinrD2DSignal_ = SIMSIGNAL_NULL;

  public:
    void initialize(int stage) override;

    // ---- ID2dChannelModel ----
    std::vector<double> getRSRP_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId = NODEID_NONE) override;
    std::vector<double> getSINR_D2D(LteAirFrame *frame, UserControlInfo *lteInfo_1, MacNodeId destId, inet::Coord destCoord, MacNodeId enbId, const std::vector<double>& rsrpVector) override;

    virtual bool isD2DInterferenceEnabled() { return enableD2DInterference_; }
    bool recordsUlTransmissionMap() override { return isUplinkInterferenceEnabled() || enableD2DInterference_; }

    /*
     * The signal a D2D reception is reported under, for RadioMedium's
     * emitRcvdSinr(), reached through this endpoint's own
     * RadioDescriptor::d2dEndpoint.
     */
    simsignal_t getRcvdSinrD2DSignal() const { return rcvdSinrD2DSignal_; }
};

} //namespace

#endif /* STACK_D2D_PHY_CHANNELMODEL_D2DCHANNELMODEL_H_ */
