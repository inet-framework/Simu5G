//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include <inet/common/ModuleAccess.h>
#include "simu5g/multileg/MlLegSelection.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlLegSelection);

MlLegSelection::~MlLegSelection()
{
    delete legPolicy_;
}

void MlLegSelection::initialize(int stage)
{
    LegSelectionBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);

        cModule *node = inet::getContainingNode(this);
        if (nodeType_ == UE) {
            legNodeIds_.push_back(MacNodeId(node->par("macNodeId").intValue()));                       // leg 0 (LTE)
            legNodeIds_.push_back(node->hasPar("nrMacNodeId")
                    ? MacNodeId(node->par("nrMacNodeId").intValue()) : NODEID_NONE);                   // leg 1 (NR)
            for (int leg = 2; ; leg++) {
                std::string parName = "nrMacNodeId" + std::to_string(leg);
                if (!node->hasPar(parName.c_str()))
                    break;
                legNodeIds_.push_back(MacNodeId(node->par(parName.c_str()).intValue()));               // leg k
            }
        }
        else {
            nodeId_ = MacNodeId(node->par("macNodeId").intValue());
        }

        legPolicy_ = makePolicyExpression(par("legPolicy"));
    }
}

int MlLegSelection::selectLeg(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService)
{
    if (nodeType_ != UE) {
        // base station: the leg of the UE id this station serves
        MacNodeId ueId = binder_->getUeIdServedBy(destAddr, nodeId_);
        if (ueId == NODEID_NONE) {
            EV << "MlLegSelection: destination is not served here. Delete packet." << endl;
            return DROP_PACKET;
        }
        return binder_->getLegOfNode(ueId);
    }

    // UE: availability = the leg is present and attached to a serving node
    auto legAvailable = [&](int leg) {
        return leg >= 0 && leg < (int)legNodeIds_.size() && legNodeIds_[leg] != NODEID_NONE
               && binder_->getServingNodeOrSelf(legNodeIds_[leg]) != NODEID_NONE;
    };

    int firstAvailable = -1;
    int numAvailable = 0;
    for (int leg = 0; leg < (int)legNodeIds_.size(); leg++)
        if (legAvailable(leg)) {
            if (firstAvailable < 0)
                firstAvailable = leg;
            numAvailable++;
        }

    if (numAvailable == 0) {
        EV << "MlLegSelection: UE is not attached to any serving node. Delete packet." << endl;
        return DROP_PACKET;
    }
    if (numAvailable == 1)
        return firstAvailable;

    computePacketOrdinal(srcAddr, destAddr, typeOfService);
    int leg = legPolicy_->intValue();
    if (!legAvailable(leg)) {
        EV << "MlLegSelection: policy chose unavailable leg " << leg << "; using leg " << firstAvailable << endl;
        leg = firstAvailable;
    }
    return leg;
}

} // namespace simu5g
