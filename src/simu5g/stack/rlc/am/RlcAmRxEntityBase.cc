//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
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
simsignal_t RlcAmRxEntityBase::rlcPacketLossSignal_[2] = { registerSignal("rlcPacketLossDl"), registerSignal("rlcPacketLossUl") };
simsignal_t RlcAmRxEntityBase::rlcPduPacketLossSignal_[2] = { registerSignal("rlcPduPacketLossDl"), registerSignal("rlcPduPacketLossUl") };
simsignal_t RlcAmRxEntityBase::rlcDelaySignal_[2] = { registerSignal("rlcDelayDl"), registerSignal("rlcDelayUl") };
simsignal_t RlcAmRxEntityBase::rlcThroughputSignal_[2] = { registerSignal("rlcThroughputDl"), registerSignal("rlcThroughputUl") };
simsignal_t RlcAmRxEntityBase::rlcPduDelaySignal_[2] = { registerSignal("rlcPduDelayDl"), registerSignal("rlcPduDelayUl") };
simsignal_t RlcAmRxEntityBase::rlcPduThroughputSignal_[2] = { registerSignal("rlcPduThroughputDl"), registerSignal("rlcPduThroughputUl") };

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

void RlcAmRxEntityBase::initAckFlowControlInfo(const FlowControlInfo *orig)
{
    ackFlowControlInfo_ = orig->dup();
    ackFlowControlInfo_->setSourceId(orig->getDestId());
    ackFlowControlInfo_->setDestId(orig->getSourceId());
    // The reverse bearer of a sidelink bearer is a sidelink bearer too, so its
    // direction does not flip; the endpoint swap above is what points the STATUS
    // PDUs back at the data sender.
    if (orig->getDirection() != SL)
        ackFlowControlInfo_->setDirection((orig->getDirection() == DL) ? UL : DL);
}

void RlcAmRxEntityBase::emitRxStatistics(bool perPdu, double throughput, simtime_t delay)
{
    if (ackFlowControlInfo_ == nullptr)
        return;

    // ackFlowControlInfo_ is the reversed flow info (it stamps the STATUS PDUs going
    // back), so the data flowed in the opposite direction to the one it names.
    // On an SLRB the direction is not reversed (there is no DL/UL pair) and the
    // bearer has no Uu bucket to record into.
    Direction dir;
    if (!uuStatsDirection(static_cast<Direction>(ackFlowControlInfo_->getDirection()), dir))
        return;
    dir = (dir == DL) ? UL : DL;

    emit(perPdu ? rlcPduThroughputSignal_[dir] : rlcThroughputSignal_[dir], throughput);
    emit(perPdu ? rlcPduDelaySignal_[dir] : rlcDelaySignal_[dir], delay.dbl());
    if (!perPdu) {
        // AM repairs everything it is given, so nothing is ever lost to the upper
        // layer. Reporting the zero makes that a measured result rather than an
        // absent one.
        emit(rlcPacketLossSignal_[dir], 0.0);
    }
}

} //namespace
