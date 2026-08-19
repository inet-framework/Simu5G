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

#include "simu5g/background/trafficGenerator/BackgroundTrafficManager.h"

namespace simu5g {

Define_Module(BackgroundTrafficManager);


BackgroundTrafficManager::~BackgroundTrafficManager()
{
    // the medium may already be gone by the time this manager is torn down;
    // mac_ itself may already be nulled out too (ModuleRefByPar clears on
    // its target's deletion), which is why cellId_ was cached at
    // registration instead of read from mac_ here
    cModule *medium = getSimulation()->getModule(mediumModuleId_);
    if (medium == nullptr)
        return;
    RadioMedium *radioMedium = check_and_cast<RadioMedium *>(medium);
    for (int i = 0; i < numBgUEs_; i++)
        radioMedium->removeBackgroundRadio(BgUeKey{cellId_, carrierFrequency_, MacNodeId(BGUE_MIN_ID + i)});
}

void BackgroundTrafficManager::initialize(int stage)
{
    BackgroundTrafficManagerBase::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        phy_.reference(this, "phyModule", true);
        mac_.reference(this, "macModule", true);
    }
    if (stage == INITSTAGE_SIMU5G_BACKGROUNDTRAFFICMANAGER) {
        // Get the reference to the channel model for the given carrier
        bsTxPower_ = phy_->getTxPwr();
        bsCoord_ = phy_->getCoord();
        channelModel_ = phy_->getChannelModel(carrierFrequency_);
        if (channelModel_ == nullptr)
            throw cRuntimeError("BackgroundTrafficManagerBase::initialize - cannot find channel model for carrier frequency %f", carrierFrequency_.get());

        // Register each background UE as a phantom radio:
        // this manager's own MacNodeId numbering is not network-unique, so
        // the medium keys phantoms by (cellId, carrierFrequency, bgUeId)
        // instead
        medium_.reference(this, "radioMediumModule", true);
        mediumModuleId_ = medium_->getId();
        cellId_ = mac_->getMacCellId();
        // Every background UE's antenna height (radio endpoint recast E4,
        // §3(k)): today's UE-side default -- no in-tree config differentiates
        // one background UE's height from another's, and none overrides
        // ueHeight where background traffic is also configured.
        constexpr double BG_UE_HEIGHT_M = 1.5;
        for (int i = 0; i < numBgUEs_; i++)
            medium_->addBackgroundRadio(BgUeKey{cellId_, carrierFrequency_, MacNodeId(BGUE_MIN_ID + i)}, bgUe_.at(i), BG_UE_HEIGHT_M);
    }
}

bool BackgroundTrafficManager::isSetBgTrafficManagerInfoInit()
{
    return !par("enablePeriodicCqiUpdate").boolValue() && par("computeAvgInterference").boolValue();
}

unsigned int BackgroundTrafficManager::getNumBands()
{
    return channelModel_->getNumBands();
}

std::vector<double> BackgroundTrafficManager::getSINR(int bgUeIndex, Direction dir, inet::Coord bgUePos, double bgUeTxPower)
{
    // This is a fictitious frame that we need to compute the SINR
    LteAirFrame *frame = new LteAirFrame("bgUeSinrComputationFrame");
    UserControlInfo *cInfo = new UserControlInfo();

    // Build a control info
    cInfo->setSourceId(MacNodeId(BGUE_MIN_ID + bgUeIndex));  // MacNodeId for the bgUe
    cInfo->setDestId(mac_->getMacNodeId());  // ID of the e/gNodeB
    cInfo->setFrameType(FEEDBACKPKT);
    cInfo->setCoord(bgUePos);
    cInfo->setDirection(dir);
    cInfo->setCarrierFrequency(carrierFrequency_);
    if (dir == UL)
        cInfo->setTxPower(bgUeTxPower);
    else
        cInfo->setTxPower(bsTxPower_);

    std::vector<double> snr = channelModel_->getSINR_bgUe(frame, cInfo);

    // Free memory
    delete frame;
    delete cInfo;

    return snr;
}

unsigned int BackgroundTrafficManager::getBackloggedUeBytesPerBlock(MacNodeId bgUeId, Direction dir)
{
    int index = num(bgUeId) - BGUE_MIN_ID;
    Cqi cqi = bgUe_.at(index)->getCqi(dir);

    // Get bytes per block based on CQI
    return mac_->getAmc()->computeBitsPerRbBackground(cqi, dir, carrierFrequency_) / 8;
}

double BackgroundTrafficManager::getTtiPeriod()
{
    return mac_->getTtiPeriod();
}

double BackgroundTrafficManager::getReceivedPower_bgUe(double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus)
{
    MacNodeId bsId = mac_->getMacNodeId();
    return channelModel_->getReceivedPower_bgUe(txPower, txPos, rxPos, dir, losStatus, bsId);
}

} //namespace
