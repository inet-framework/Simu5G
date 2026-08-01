//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include <inet/common/ModuleAccess.h>
#include "simu5g/multileg/MlRegistration.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlRegistration);

void MlRegistration::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        // collect the extra legs' ids from the node's nrMacNodeId<k> parameters
        cModule *containingNode = inet::getContainingNode(this);
        for (int leg = 2; ; leg++) {
            std::string parName = "nrMacNodeId" + std::to_string(leg);
            if (!containingNode->hasPar(parName.c_str()))
                break;
            extraLegNodeIds_[leg] = MacNodeId(containingNode->par(parName.c_str()).intValue());
        }
    }
    Registration::initialize(stage);
}

MacNodeId MlRegistration::getNodeIdOfLeg(int leg) const
{
    if (leg == LEG_LTE)
        return getLteNodeId();
    if (leg == LEG_NR)
        return getNrNodeId();
    auto it = extraLegNodeIds_.find(leg);
    return it != extraLegNodeIds_.end() ? it->second : NODEID_NONE;
}

void MlRegistration::registerNodes()
{
    Registration::registerNodes();

    cModule *node = inet::getContainingNode(this);
    for (const auto& [leg, nodeId] : extraLegNodeIds_) {
        // declare the leg BEFORE anything binds the node's IP address to the id
        getMlBinder()->setLegOfNode(nodeId, leg);
        binder->registerNode(nodeId, node, UE, /*isNr=*/true);
    }
}

void MlRegistration::registerServingNodes()
{
    Registration::registerServingNodes();

    cModule *node = inet::getContainingNode(this);
    for (const auto& [leg, nodeId] : extraLegNodeIds_) {
        std::string parName = "nrServingNodeId" + std::to_string(leg);
        MacNodeId servingNodeId = MacNodeId(node->par(parName.c_str()).intValue());
        binder->registerServingNode(servingNodeId, nodeId);
    }
}

void MlRegistration::registerMulticastGroups()
{
    Registration::registerMulticastGroups();
    // extra legs need no multicast enrollment in the demo scenarios
}

void MlRegistration::finish()
{
    Registration::finish();
    if (getSimulation()->getSimulationStage() != CTX_FINISH)
        for (const auto& [leg, nodeId] : extraLegNodeIds_)
            binder->unregisterNode(nodeId);
}

} // namespace simu5g
