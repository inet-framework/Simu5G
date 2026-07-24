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

#include "RadioLinkMonitor.h"

#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/common/InitStages.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(RadioLinkMonitor);

RadioLinkMonitor::~RadioLinkMonitor()
{
    cancelAndDelete(t310Timer_);
    cancelAndDelete(beaconLossTimer_);
}

void RadioLinkMonitor::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);
        bearerManagement_ = check_and_cast<BearerManagement *>(getParentModule()->getSubmodule("bearerManagement"));

        isNr_ = par("isNr");
        cModule *hostModule = inet::getContainingNode(this);
        nodeId_ = MacNodeId(hostModule->par(isNr_ ? "nrMacNodeId" : "macNodeId").intValue());

        enabled_ = par("enabled");
        qOut_ = par("qOut").doubleValue();
        qIn_ = par("qIn").doubleValue();
        if (qIn_ < qOut_)
            throw cRuntimeError("qIn (%g dB) must be >= qOut (%g dB)", qIn_, qOut_);
        n310_ = par("n310");
        n311_ = par("n311");
        if (n310_ < 1 || n311_ < 1)
            throw cRuntimeError("n310 and n311 must be >= 1 (got %d and %d)", n310_, n311_);
        t310_ = par("t310");
        beaconLossTimeout_ = par("beaconLossTimeout");
        if (enabled_ && (t310_ <= SIMTIME_ZERO || beaconLossTimeout_ <= SIMTIME_ZERO))
            throw cRuntimeError("t310 and beaconLossTimeout must be positive when enabled=true");
        t310Timer_ = new cMessage("rlmT310");
        beaconLossTimer_ = new cMessage("rlmBeaconLoss");

        WATCH(monitoredCell_);
        WATCH(oosCounter_);
        WATCH(isCounter_);
        WATCH(rlfDeclared_);
    }
    else if (stage == inet::INITSTAGE_LAST) {
        // The serving cell is known by now (HandoverController resolved it, including
        // dynamicCellAssociation, in INITSTAGE_SIMU5G_PHYSICAL_LAYER). Arm the beacon-loss
        // watchdog: if the serving cell broadcasts no beacons at all (misconfiguration),
        // this fails loudly at beaconLossTimeout instead of leaving radio link monitoring
        // silently inoperative.
        MacNodeId servingNodeId = binder_->getServingNode(nodeId_);
        if (enabled_ && servingNodeId != NODEID_NONE) {
            monitoredCell_ = servingNodeId;
            scheduleAfter(beaconLossTimeout_, beaconLossTimer_);
        }
    }
}

void RadioLinkMonitor::handleMessage(cMessage *msg)
{
    ASSERT(msg->isSelfMessage());

    MacNodeId servingNodeId = binder_->getServingNode(nodeId_);

    if (msg == t310Timer_) {
        // T310 expiry (TS 38.331 5.3.10.3): radio problems persisted -- radio link failure
        if (servingNodeId != NODEID_NONE && servingNodeId == monitoredCell_)
            declareRadioLinkFailure(servingNodeId, "T310 expired");
        else
            reset();  // stale timer: the serving cell changed since T310 was started
    }
    else if (msg == beaconLossTimer_) {
        if (numBeaconsSeen_ == 0)
            throw cRuntimeError("Radio link monitoring is enabled but no beacon was ever received "
                    "from the serving cell (node %d) within beaconLossTimeout: the serving eNB/gNB "
                    "is not broadcasting beacons -- enable beacon broadcasting there, or set enabled=false",
                    (int)num(servingNodeId));
        if (!rlfDeclared_ && servingNodeId != NODEID_NONE && servingNodeId == monitoredCell_)
            declareRadioLinkFailure(servingNodeId, "beacon loss (no serving-cell beacon within beaconLossTimeout)");
    }
    else
        throw cRuntimeError("RadioLinkMonitor::handleMessage: unknown self-message '%s'", msg->getName());
}

void RadioLinkMonitor::servingCellBeaconReceived(MacNodeId servingNodeId, double rssi)
{
    Enter_Method("servingCellBeaconReceived");
    ASSERT(enabled_);  // the caller computes the RSSI only when monitoring is enabled

    numBeaconsSeen_++;

    if (monitoredCell_ != servingNodeId) {
        // first beacon from a new serving cell (attachment or completed handover):
        // restart monitoring from a clean slate
        reset();
        monitoredCell_ = servingNodeId;
    }

    // re-arm the beacon-loss watchdog
    rescheduleAfter(beaconLossTimeout_, beaconLossTimer_);

    if (rlfDeclared_) {
        // RLF already indicated to RRC; resume monitoring once the link has recovered
        if (rssi >= qIn_) {
            EV << NOW << " RadioLinkMonitor - UE " << nodeId_ << " link to " << servingNodeId
               << " recovered (RSSI " << rssi << " dB >= qIn), monitoring resumed" << endl;
            rlfDeclared_ = false;
            oosCounter_ = isCounter_ = 0;
        }
        return;
    }

    if (rssi < qOut_) {
        // out-of-sync indication
        isCounter_ = 0;
        oosCounter_++;
        EV << NOW << " RadioLinkMonitor - UE " << nodeId_ << " out-of-sync from " << servingNodeId
           << " (RSSI " << rssi << " dB < qOut " << qOut_ << " dB), " << oosCounter_ << "/" << n310_ << endl;
        if (oosCounter_ >= n310_ && !t310Timer_->isScheduled()) {
            EV << NOW << " RadioLinkMonitor - UE " << nodeId_ << " starting T310 (" << t310_ << "s)" << endl;
            isCounter_ = 0;
            scheduleAfter(t310_, t310Timer_);
        }
    }
    else if (rssi >= qIn_) {
        // in-sync indication
        oosCounter_ = 0;
        if (t310Timer_->isScheduled() && ++isCounter_ >= n311_) {
            EV << NOW << " RadioLinkMonitor - UE " << nodeId_ << " in-sync x " << n311_
               << " while T310 running: link recovered, stopping T310" << endl;
            cancelEvent(t310Timer_);
            isCounter_ = 0;
        }
    }
    // else: between qOut and qIn -- no indication, counters unchanged (TS 38.133 hysteresis band)
}

void RadioLinkMonitor::declareRadioLinkFailure(MacNodeId servingNodeId, const char *cause)
{
    EV << NOW << " RadioLinkMonitor::declareRadioLinkFailure - UE " << nodeId_ << ", serving cell "
       << servingNodeId << ": " << cause << endl;
    rlfDeclared_ = true;
    oosCounter_ = isCounter_ = 0;
    cancelEvent(t310Timer_);
    // Hand over to the RRC teardown machinery: releases this bearer's MAC/RLC/PDCP state on
    // both ends; Ip2Nic then models RRC re-establishment via its T311/T301 timers (or
    // release-to-IDLE when t311 = 0).
    bearerManagement_->scheduleRadioLinkFailure(servingNodeId, isNr_);
}

void RadioLinkMonitor::reset()
{
    oosCounter_ = isCounter_ = 0;
    rlfDeclared_ = false;
    cancelEvent(t310Timer_);
}

} //namespace
