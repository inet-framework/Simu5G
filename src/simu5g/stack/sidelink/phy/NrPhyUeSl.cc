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

#include "simu5g/stack/sidelink/phy/NrPhyUeSl.h"

#include "simu5g/stack/sidelink/common/SlBinder.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrPhyUeSl);

simsignal_t NrPhyUeSl::uuHalfDuplexSlDropsSignal_ = registerSignal("uuHalfDuplexSlDrops");
simsignal_t NrPhyUeSl::slUuTxConflictsSignal_ = registerSignal("slUuTxConflicts");

void NrPhyUeSl::initialize(int stage)
{
    NrPhyUe::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        sharedUuSlRadio_ = par("sharedUuSlRadio");
    }
    else if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS && sharedUuSlRadio_) {
        // the shared radio state is created by SlRrc at INITSTAGE_LOCAL
        radioState_ = SlBinder::getInstance()->getUeRadioState(nodeId_);
        if (radioState_ == nullptr)
            throw cRuntimeError("NrPhyUeSl: sharedUuSlRadio=true but node %hu has no radio state "
                                "(is the sidelink leg present?)", num(nodeId_));
        WATCH(uuHalfDuplexSlDrops_);
    }
}

void NrPhyUeSl::recordUuTx(simtime_t duration)
{
    bool conflict = radioState_->recordTx(SlUeRadioState::UU, NOW, NOW + duration);
    if (conflict) {
        // TX-TX conflicts are counted, not suppressed (D32; a TX-defer
        // policy belongs with sync modeling in SL-4)
        emit(slUuTxConflictsSignal_, 1L);
        EV << NOW << " NrPhyUeSl::recordUuTx - Uu TX overlaps an SL TX (counted, not suppressed)" << endl;
    }
}

void NrPhyUeSl::handleUpperMessage(cMessage *msg)
{
    if (sharedUuSlRadio_) {
        // every UL airframe counts as a Uu TX (data, RAC, BSR-only PDUs);
        // duration mirrors LtePhyBase::handleUpperMessage's slot duration
        auto pkt = check_and_cast<inet::Packet *>(msg);
        auto lteInfo = pkt->getTag<UserControlInfo>();
        NumerologyIndex numerologyIndex = binder_->getNumerologyIndexFromCarrierFreq(lteInfo->getCarrierFrequency());
        recordUuTx(binder_->getSlotDurationFromNumerologyIndex(numerologyIndex));
    }

    NrPhyUe::handleUpperMessage(msg);
}

void NrPhyUeSl::sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req)
{
    if (sharedUuSlRadio_) {
        Enter_Method("sendFeedback");
        recordUuTx(TTI);  // the CQI feedback airframe's duration (LtePhyUe::sendFeedback)
    }

    NrPhyUe::sendFeedback(fbDl, fbUl, req);
}

void NrPhyUeSl::handleAirFrame(cMessage *msg)
{
    if (sharedUuSlRadio_) {
        // drop an arriving Uu DATA frame whose reception interval
        // [arrival - duration, arrival] overlaps an SL transmission; Uu
        // control frames (GRANTPKT/RACPKT/HARQPKT/FEEDBACKPKT/beacons)
        // stay lossless, consistent with the Uu control model (G6)
        auto *frame = check_and_cast<LteAirFrame *>(msg);
        auto *ci = dynamic_cast<UserControlInfo *>(frame->getControlInfo());
        if (ci != nullptr && ci->getFrameType() == DATAPKT &&
            radioState_->overlapsTx(SlUeRadioState::SL, NOW - frame->getDuration(), NOW))
        {
            uuHalfDuplexSlDrops_++;
            emit(uuHalfDuplexSlDropsSignal_, 1L);
            EV << NOW << " NrPhyUeSl::handleAirFrame - Uu data frame lost: the SL leg was "
               << "transmitting during its reception (half-duplex, D32)" << endl;
            delete msg;
            return;
        }
    }

    NrPhyUe::handleAirFrame(msg);
}

} // namespace simu5g
