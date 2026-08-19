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

#ifndef BACKGROUNDTRAFFICMANAGER_H_
#define BACKGROUNDTRAFFICMANAGER_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/blerCurves/PhyPisaData.h"
#include "simu5g/background/trafficGenerator/BackgroundTrafficManagerBase.h"
#include "simu5g/background/trafficGenerator/generators/TrafficGeneratorBase.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/phy/PhyEnb.h"
#include "simu5g/stack/phy/channelmodel/RadioMedium.h"

namespace simu5g {

using namespace omnetpp;

class TrafficGeneratorBase;
class LteMacEnb;
class ChannelModelBase;

//
// BackgroundTrafficManager
//
class BackgroundTrafficManager : public BackgroundTrafficManagerBase
{
  protected:
    // references to the MAC and PHY layer of the e/gNodeB
    inet::ModuleRefByPar<LteMacEnb> mac_;

    // reference to phy module
    inet::ModuleRefByPar<PhyEnb> phy_;

    // reference to the channel model for the given carrier
    ChannelModelBase *channelModel_ = nullptr;

    // the medium each of this manager's background UEs registers a phantom
    // radio with; this NIC-side manager has a MAC/cell
    // identity to key phantoms by, unlike BackgroundCellTrafficManager and
    // BackgroundScheduler, which do not register
    inet::ModuleRefByPar<RadioMedium> medium_;

    // medium_'s module id, cached so the destructor can find it without
    // dereferencing medium_ itself, which may already be gone by teardown
    // (mirrors StochasticChannelModel's own registration/deregistration)
    int mediumModuleId_ = -1;

    // the owning e/gNodeB's cell id, cached at registration for the same
    // reason: mac_ (a ModuleRefByPar) may already be nulled out by the time
    // the destructor runs, if the MAC module is torn down first
    MacCellId cellId_ = NODEID_NONE;

  protected:
    void initialize(int stage) override;

    double getTtiPeriod() override;
    bool isSetBgTrafficManagerInfoInit() override;
    std::vector<double> getSINR(int bgUeIndex, Direction dir, inet::Coord bgUePos, double bgUeTxPower) override;

  public:
    ~BackgroundTrafficManager() override;

    // get the number of RBs
    unsigned int getNumBands() override;

    // returns the bytes per block of the given UE in the given direction
    unsigned int getBackloggedUeBytesPerBlock(MacNodeId bgUeId, Direction dir) override;

    // Compute received power for a background UE according to path loss
    double getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus) override;
};

} //namespace

#endif

