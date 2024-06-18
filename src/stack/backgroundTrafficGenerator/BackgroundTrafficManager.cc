//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "stack/backgroundTrafficGenerator/BackgroundTrafficManager.h"

namespace simu5g {

Define_Module(BackgroundTrafficManager);

void BackgroundTrafficManager::initialize(int stage)
{
    BackgroundTrafficManagerBase::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL)
    {
        phy_.reference(this, "phyModule", true);
    }
    if (stage == inet::INITSTAGE_LAST-1)
    {
        // get the reference to the channel model for the given carrier
        bsTxPower_ = phy_->getTxPwr();
        bsCoord_ = phy_->getCoord();
        channelModel_ = phy_->getChannelModel(carrierFrequency_);
        if (channelModel_ == nullptr)
            throw cRuntimeError("BackgroundTrafficManagerBase::initialize - cannot find channel model for carrier frequency %f", carrierFrequency_);
    }
}

} //namespace

