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

#ifndef STACK_PHY_CHANNELMODEL_SIONNACHANNELMODEL_H_
#define STACK_PHY_CHANNELMODEL_SIONNACHANNELMODEL_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"
#include "simu5g/stack/phy/channelmodel/sionna/SionnaManager.h"

namespace simu5g {

using namespace omnetpp;

//
// Channel model whose propagation is sourced from the ray-traced SionnaManager
// table (Plan A). It reuses the LteRealisticChannelModel interference + noise
// aggregation but overrides getSINR()/getRSRP() to take the desired-signal
// per-(link, RB) path gain from the Sionna table, and overrides getAttenuation()
// so the inherited interference routines source from the table as well. The
// analytic antenna gain / cable loss / fading / shadowing / angular terms are
// neutralized because the Sionna path gain already spans Tx port -> Rx port.
//
class SionnaChannelModel : public LteRealisticChannelModel
{
  protected:
    inet::ModuleRefByPar<SionnaManager> sionnaManager_;

    // desired-link received power per band [dBm], built from the Sionna path gain
    std::vector<double> desiredRecvPowerPerBand(double txPower, const inet::Coord& thisPos,
            const inet::Coord& otherPos);
    // mean path gain [dB] for the link between two coordinates (for interference reuse)
    double meanPathGainDb(const inet::Coord& a, const inet::Coord& b);

  public:
    void initialize(int stage) override;

    // --- propagation sourced from the Sionna table ---
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl) override;
    double getAttenuation_D2D(MacNodeId nodeId, Direction dir, inet::Coord coord,
            MacNodeId node2_Id, inet::Coord coord_2, bool cqiDl) override;

    // neutralize analytic terms already included in the Sionna path gain
    double computeShadowing(double sqrDistance, MacNodeId nodeId, double speed, bool cqiDl) override;
    double computeAngularAttenuation(double hAngle, double vAngle = 0) override;

    // --- aggregation: desired per-RB from the table, interference/noise reused ---
    std::vector<double> getSINR(LteAirFrame *frame, UserControlInfo *lteInfo) override;
    std::vector<double> getRSRP(LteAirFrame *frame, UserControlInfo *lteInfo) override;

    double getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos,
            Direction dir, bool losStatus, MacNodeId bsId) override;
};

} //namespace

#endif
