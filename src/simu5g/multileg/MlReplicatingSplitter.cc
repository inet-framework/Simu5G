//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include <inet/common/ModuleAccess.h>
#include <inet/common/packet/Packet.h>
#include "simu5g/multileg/MlReplicatingSplitter.h"
#include "simu5g/common/LteControlInfo.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlReplicatingSplitter);

void MlReplicatingSplitter::initialize(int stage)
{
    PdcpLegSplitter::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");
        replicaSentSignal_ = registerSignal("replicaSent");

        binder_.reference(this, "binderModule", true);

        cModule *node = inet::getContainingNode(this);
        MacNodeId ownId = MacNodeId(node->par("macNodeId").intValue());
        isUe_ = (getNodeTypeById(ownId) == UE);

        // bearer leg k rides stack leg firstLeg+k (the NR legs, 1..numLegs)
        int firstLeg = par("firstStackLeg");
        for (int k = 0; k < numLegs_; k++)
            stackLegs_.push_back(firstLeg + k);

        if (isUe_)
            for (int leg : stackLegs_)
                legNodeIds_.push_back(binder_->getPeerLegId(ownId, leg));
    }
}

void MlReplicatingSplitter::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);

    // Replicate: one copy per connected leg, each addressed to that leg.
    // The last leg consumes the original.
    for (int legIdx = 0; legIdx < numLegs_; legIdx++) {
        if (!gate("out", legIdx)->isConnected())
            continue;
        bool last = true;
        for (int rest = legIdx + 1; rest < numLegs_; rest++)
            if (gate("out", rest)->isConnected())
                last = false;

        auto *replica = last ? pkt : pkt->dup();
        adaptIdsForLeg(replica, legIdx);
        emit(replicaSentSignal_, replica);
        emit(sentPacketToLowerLayerSignal_, replica);
        send(replica, "out", legIdx);
        if (last)
            return;
    }

    // no leg connected at all
    EV_WARN << NOW << " MlReplicatingSplitter - no leg available; dropping PDU" << endl;
    delete pkt;
}

void MlReplicatingSplitter::adaptIdsForLeg(inet::Packet *pkt, int legIdx)
{
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    int stackLeg = stackLegs_[legIdx];

    if (isUe_) {
        // UL: this UE's id on the leg; the peer (gNB) is the same for all legs
        MacNodeId srcId = legNodeIds_[legIdx];
        lteInfo->setSourceId(srcId);
        lteInfo->setDestId(binder_->getServingNodeOrSelf(srcId));
    }
    else {
        // DL: the peer UE's id on the leg; the source (this gNB) is the same for all legs
        MacNodeId destId = binder_->getPeerLegId(lteInfo->getDestId(), stackLeg);
        if (destId != NODEID_NONE)
            lteInfo->setDestId(destId);
    }
}

} // namespace simu5g
