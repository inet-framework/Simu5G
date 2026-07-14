//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIDELINK_TR37885CHANNELMODEL_H_
#define _SIDELINK_TR37885CHANNELMODEL_H_

#include <map>

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/phy/ISlChannelModel.h"

namespace simu5g {

/**
 * TR 37.885 V2V sidelink channel model (design decision D7): highway/urban
 * pathloss with per-pair log-normal shadowing and optional NLOSv vehicle
 * blockage loss; SINR from the SL transmission map in SlBinder (co-slot,
 * co-subchannel interferers) plus thermal noise; PSCCH decode by SINR
 * threshold (D11); PSSCH decode via the per-CQI BLER curves.
 *
 * Free-standing implementation (the D7 alternative): subclassing the Uu
 * channel-model hierarchy would tie the SL carrier to a ComponentCarrier
 * entry and Binder's Uu carrier registry (gap G8), for machinery (DAS,
 * feedback, Uu scenarios) sidelink does not use. Fast fading is not modeled
 * in SL-1.
 *
 * Pathloss constants transcribed from TR 37.885 §6.2.1 (V2V, fc in GHz,
 * d in m):
 *   highway LOS/NLOSv:  PL = 32.4  + 20.0*log10(d) + 20.0*log10(fc), sigma 3 dB
 *   urban LOS:          PL = 38.77 + 16.7*log10(d) + 18.2*log10(fc), sigma 3 dB
 *   urban NLOS:         PL = 36.85 + 30.0*log10(d) + 18.9*log10(fc), sigma 4 dB
 *   NLOSv extra loss:   A ~ N(max(0, 15*log10(d) - 41), 4.5^2) dB, drawn per frame
 */
class Tr37885ChannelModel : public omnetpp::cSimpleModule, public ISlChannelModel
{
  protected:
    inet::ModuleRefByPar<Binder> binder_;   // for the BLER curves (phyPisaData) only
    SlBinder *slBinder_ = nullptr;

    enum Scenario { HIGHWAY, URBAN } scenario_ = HIGHWAY;
    enum LosState { LOS, NLOSV, NLOS } losState_ = LOS;

    bool shadowing_ = true;
    double noiseFigureDb_ = 9;
    double pscchSinrThresholdDb_ = 0;
    double cableLossDb_ = 0;
    double harqReduction_ = 0.2;

    // persistent per-pair shadowing values [dB], keyed (min nodeId, max nodeId)
    std::map<std::pair<MacNodeId, MacNodeId>, double> shadowingMap_;

    void initialize(int stage) override;
    int numInitStages() const override;
    void handleMessage(omnetpp::cMessage *msg) override;

    /// pathloss + (persistent) shadowing + (per-call) blockage between two nodes [dB]
    double computeAttenuationDb(MacNodeId txId, MacNodeId rxId, const inet::Coord& txCoord, const inet::Coord& rxCoord, double fcGHz);
    double computePathLossDb(double distance, double fcGHz) const;
    double getShadowing(MacNodeId txId, MacNodeId rxId, double distance);

    /// thermal noise over the frame's subchannels [dBm]
    double computeNoiseDbm(const SlBinder::SlCarrierInfo *carrier, int numSubchannels) const;

  public:
    SlReceptionResult computeReception(const SlAirFrameInfo& info, const inet::Coord& rxCoord, MacNodeId rxNodeId) override;
    double getHarqReduction() const override { return harqReduction_; }
};

} // namespace simu5g

#endif
