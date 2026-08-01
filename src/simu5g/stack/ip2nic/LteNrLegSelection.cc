#include <inet/networklayer/common/NetworkInterface.h>
#include "simu5g/stack/ip2nic/LteNrLegSelection.h"
#include "simu5g/stack/ip2nic/HandoverPacketHolderUe.h"

namespace simu5g {

using namespace inet;

Define_Module(LteNrLegSelection);

LteNrLegSelection::~LteNrLegSelection()
{
    delete dcLegPolicy_;
    delete legPolicy_;
}

void LteNrLegSelection::initialize(int stage)
{
    LegSelectionBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        binder_.reference(this, "binderModule", true);

        auto *networkIf = getContainingNicModule(this);
        dualConnectivityEnabled_ = networkIf->par("dualConnectivityEnabled").boolValue();

        if (nodeType_ == NODEB) {
            cModule *bs = getContainingNode(this);
            nodeId_ = MacNodeId(bs->par("macNodeId").intValue());
        }
        else if (nodeType_ == UE) {
            cModule *ue = getContainingNode(this);
            nodeId_ = MacNodeId(ue->par("macNodeId").intValue());
            if (ue->hasPar("nrMacNodeId"))
                nrNodeId_ = MacNodeId(ue->par("nrMacNodeId").intValue());
        }

        // Set up the policy expressions
        dcLegPolicy_ = makePolicyExpression(par("dcLegPolicy"));
        legPolicy_ = makePolicyExpression(par("legPolicy"));
    }
}

int LteNrLegSelection::selectLeg(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService)
{
    bool lteLegAvailable, nrLegAvailable;

    if (nodeType_ == NODEB) {
        MacNodeId ueId = binder_->getMacNodeId(destAddr);
        MacNodeId nrUeId = binder_->getNrMacNodeId(destAddr);
        lteLegAvailable = (binder_->getServingNodeOrSelf(ueId) != NODEID_NONE);
        nrLegAvailable = (binder_->getServingNodeOrSelf(nrUeId) != NODEID_NONE);
    }
    else { // UE
        // KLUDGE: use HandoverPacketHolder's serving node IDs instead of binder's,
        // to prevent a runtime error in one of the simulations:
        //
        //    test_numerology, multicell_CBR_UL, ue[9], t=0.001909132428, event #475 (HandoverPacketHolder), #476 (Ip2Nic)
        //    after: ueLteStack=true, ueNrStack=true, servingNodeId=1, nrServingNodeId=2, typeOfService=10 --> leg = 1
        //    before: ueLteStack=true, ueNrStack=true,servingNodeId=1, nrServingNodeId=0, typeOfService=10 --> leg = 0
        //
        auto handoverPacketHolder = inet::getModuleFromPar<HandoverPacketHolderUe>(par("handoverPacketHolderModule"), this);
        lteLegAvailable = (handoverPacketHolder->getServingNodeId() != NODEID_NONE);
        nrLegAvailable = (handoverPacketHolder->getNrServingNodeId() != NODEID_NONE);
    }

    if (!lteLegAvailable && !nrLegAvailable) {
        EV << "LteNrLegSelection: UE is not attached to any serving node. Delete packet." << endl;
        return DROP_PACKET;
    }
    else if (!nrLegAvailable)
        return LEG_LTE;
    else if (!lteLegAvailable)
        return LEG_NR;
    else if (dualConnectivityEnabled_) {
        computePacketOrdinal(srcAddr, destAddr, typeOfService);
        return dcLegPolicy_->intValue();
    }
    else {
        computePacketOrdinal(srcAddr, destAddr, typeOfService);
        return legPolicy_->intValue();
    }
}

} //namespace
