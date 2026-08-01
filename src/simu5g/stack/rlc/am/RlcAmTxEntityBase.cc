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

#include "simu5g/stack/rlc/am/RlcAmTxEntityBase.h"
#include "simu5g/stack/rrc/BearerManagement.h"

namespace simu5g {

// Registered here (single translation unit) so the signal-id assignment order is
// independent of the LTE/NR concrete .cc files.
simsignal_t RlcAmTxEntityBase::wastedGrantedBytesSignal_ = registerSignal("wastedGrantedBytes");
simsignal_t RlcAmTxEntityBase::enqueuedSduSizeSignal_ = registerSignal("enqueuedSduSize");
simsignal_t RlcAmTxEntityBase::enqueuedSduRateSignal_ = registerSignal("enqueuedSduRate");
simsignal_t RlcAmTxEntityBase::requestedPduSizeSignal_ = registerSignal("requestedPduSize");
simsignal_t RlcAmTxEntityBase::txWindowOccupationSignal_ = registerSignal("txWindowOccupation");
simsignal_t RlcAmTxEntityBase::txWindowFullSignal_ = registerSignal("txWindowFull");
simsignal_t RlcAmTxEntityBase::retransmissionPduSignal_ = registerSignal("retransmissionPdu");
simsignal_t RlcAmTxEntityBase::receivedPacketFromUpperLayerSignal_ = registerSignal("receivedPacketFromUpperLayer");
simsignal_t RlcAmTxEntityBase::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");

void RlcAmTxEntityBase::declareRadioLinkFailure()
{
    if (radioLinkFailureDetected_)
        return;

    EV << getFullPath() << " - radio link failure: a PDU reached maxRtxThreshold ("
       << maxRtxThreshold_ << ") retransmissions" << endl;
    radioLinkFailureDetected_ = true;

    // Indicate the failure to RRC/BearerManagement (TS 38.322 5.3.2 / TS 36.322
    // 5.2.1): it tears down this bearer's MAC/RLC/PDCP state at a safe point.
    if (lteInfo_) {
        auto *bm = inet::getModuleFromPar<BearerManagement>(par("bearerManagementModule"), this);
        bm->scheduleRadioLinkFailure(lteInfo_->getDestId(), bm->legOfBearer(lteInfo_));
    }
}

} //namespace
