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

#include "simu5g/stack/d2d/rrc/HandoverControllerD2D.h"

#include "simu5g/stack/d2d/phy/LtePhyUeD2D.h"
#include "simu5g/stack/d2d/phy/NrPhyUeD2D.h"
#include "simu5g/stack/d2d/rrc/D2dModeSelectionBase.h"
#include "simu5g/stack/mac/LteMacEnb.h"
#include "simu5g/stack/mac/LteMacUe.h"
#include "simu5g/stack/d2d/mac/ID2dAmc.h"
#include "simu5g/stack/ip2nic/HandoverPacketHolderUe.h"
#include "simu5g/stack/phy/feedback/LteDlFeedbackGenerator.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(HandoverControllerD2D);

void HandoverControllerD2D::onHandoverStarting()
{
    // D2D-specific: Perform D2D mode switch before handover (both LTE and NR D2D variants)
    if (dynamic_cast<LtePhyUeD2D*>(phy_) || dynamic_cast<NrPhyUeD2D*>(phy_)) {
        if (servingNodeId_ != NODEID_NONE) {
            // Stop active D2D flows (go back to Infrastructure mode)
            // Currently, DM is possible only for UEs served by the same cell

            // Trigger D2D mode switch (the serving eNB may not be D2D-capable, in which case there is nothing to do)
            cModule *enb = binder_->getNodeModule(servingNodeId_);
            D2dModeSelectionBase *d2dModeSelection = dynamic_cast<D2dModeSelectionBase *>(enb->getSubmodule("cellularNic")->getSubmodule("rrc")->getSubmodule("d2dModeSelection"));
            if (d2dModeSelection != nullptr)
                d2dModeSelection->doModeSwitchAtHandover(nodeId_, false);
            else
                EV_WARN << "HandoverControllerD2D: serving eNB " << servingNodeId_ << " is not D2D-capable - no D2D mode switch before handover" << endl;
        }
    }
}

void HandoverControllerD2D::onHandoverExecuting()
{
    // D2D-specific: detach/attach the D2D direction on the old/new AMC (before common logic).
    // Covers both the LTE and NR D2D PHY variants (clean NrPhyUe is no longer a LtePhyUeD2D).
    if (dynamic_cast<LtePhyUeD2D*>(phy_) || dynamic_cast<NrPhyUeD2D*>(phy_)) {
        if (servingNodeId_ != NODEID_NONE) {
            LteAmc *oldAmc = check_and_cast<LteMacEnb *>(binder_->getMacFromMacNodeId(servingNodeId_))->getAmc();
            // The old serving eNB may not be D2D-capable (its AMC would throw on a D2D direction); skip if so.
            if (dynamic_cast<ID2dAmc *>(oldAmc) != nullptr)
                oldAmc->detachUser(nodeId_, D2D);
            else
                EV_WARN << "HandoverControllerD2D: serving eNB " << servingNodeId_ << " is not D2D-capable - skipping D2D AMC detach" << endl;
        }

        if (candidateServingNodeId_ != NODEID_NONE) {
            LteAmc *newAmc = getAmcModule(candidateServingNodeId_);
            // The new serving eNB may not be D2D-capable (its AMC would throw on a D2D direction); skip if so.
            if (dynamic_cast<ID2dAmc *>(newAmc) != nullptr)
                newAmc->attachUser(nodeId_, D2D);
            else
                EV_WARN << "HandoverControllerD2D: candidate serving eNB " << candidateServingNodeId_ << " is not D2D-capable - skipping D2D AMC attach" << endl;
        }

        // Schedule the post-handover D2D mode re-selection. In the pre-split core this was
        // scheduled near the tail of HandoverController::doHandover(), guarded by the same PHY
        // capability check and by 'servingNodeId_ != NODEID_NONE' evaluated AFTER servingNodeId_
        // had been set to the candidate -- i.e. exactly 'candidateServingNodeId_ != NODEID_NONE'
        // here. doHandover() inserts no other NOW/priority-10 event between this hook and the old
        // site, so the message's FES ordering is unchanged (self-message runs at end of the TTI).
        if (candidateServingNodeId_ != NODEID_NONE) {
            cMessage *msg = new cMessage("doModeSwitchAtHandover");
            msg->setSchedulingPriority(10);
            scheduleAt(NOW, msg);
        }
    }
}

void HandoverControllerD2D::onHandoverCompleted()
{
    // Handover completed: ask the new serving cell's mode selection module whether
    // D2D connections of this UE can switch (back) to Direct Mode. The serving eNB may
    // not be D2D-capable (no d2dModeSelection submodule), in which case there is nothing to do.
    cModule *enb = binder_->getNodeModule(servingNodeId_);
    D2dModeSelectionBase *d2dModeSelection = dynamic_cast<D2dModeSelectionBase *>(enb->getSubmodule("cellularNic")->getSubmodule("rrc")->getSubmodule("d2dModeSelection"));
    if (d2dModeSelection != nullptr)
        d2dModeSelection->doModeSwitchAtHandover(nodeId_, true);
    else
        EV_WARN << "HandoverControllerD2D: serving eNB " << servingNodeId_ << " is not D2D-capable - no D2D mode switch at handover completion" << endl;
}

} //namespace
