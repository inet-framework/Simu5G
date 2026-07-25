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

#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/NetworkInterface.h>

#include "simu5g/stack/rlc/am/RlcAmRxEntityBase.h"

namespace simu5g {

using namespace inet;

// LTE statistics
simsignal_t RlcAmRxEntityBase::rlcCellPacketLossSignal_[2] = { registerSignal("rlcCellPacketLossDl"), registerSignal("rlcCellPacketLossUl") };
simsignal_t RlcAmRxEntityBase::rlcPacketLossSignal_[2] = { registerSignal("rlcPacketLossDl"), registerSignal("rlcPacketLossUl") };
simsignal_t RlcAmRxEntityBase::rlcPduPacketLossSignal_[2] = { registerSignal("rlcPduPacketLossDl"), registerSignal("rlcPduPacketLossUl") };
simsignal_t RlcAmRxEntityBase::rlcDelaySignal_[2] = { registerSignal("rlcDelayDl"), registerSignal("rlcDelayUl") };
simsignal_t RlcAmRxEntityBase::rlcThroughputSignal_[2] = { registerSignal("rlcThroughputDl"), registerSignal("rlcThroughputUl") };
simsignal_t RlcAmRxEntityBase::rlcPduDelaySignal_[2] = { registerSignal("rlcPduDelayDl"), registerSignal("rlcPduDelayUl") };
simsignal_t RlcAmRxEntityBase::rlcPduThroughputSignal_[2] = { registerSignal("rlcPduThroughputDl"), registerSignal("rlcPduThroughputUl") };
simsignal_t RlcAmRxEntityBase::rlcCellThroughputSignal_[2] = { registerSignal("rlcCellThroughputDl"), registerSignal("rlcCellThroughputUl") };

// NR statistics (declared only by the NrRlcAmRxEntity profile)
simsignal_t RlcAmRxEntityBase::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t RlcAmRxEntityBase::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");
simsignal_t RlcAmRxEntityBase::rxWindowOccupationSignal_ = registerSignal("rxWindowOccupation");

void RlcAmRxEntityBase::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        initMode();
    }
}

} //namespace
