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

#include "simu5g/stack/rlc/am/RlcAmTxEntityBase.h"

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

} //namespace
