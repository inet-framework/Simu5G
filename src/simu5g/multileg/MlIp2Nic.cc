//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include <inet/common/ModuleAccess.h>
#include "simu5g/multileg/MlIp2Nic.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlIp2Nic);

void MlIp2Nic::initialize(int stage)
{
    Ip2Nic::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL && nodeType_ == UE) {
        cModule *node = inet::getContainingNode(this);
        for (int leg = 2; ; leg++) {
            std::string parName = "nrMacNodeId" + std::to_string(leg);
            if (!node->hasPar(parName.c_str()))
                break;
            extraLegIds_[leg] = MacNodeId(node->par(parName.c_str()).intValue());
        }
    }
}

MacNodeId MlIp2Nic::getLocalIdOfLeg(int leg) const
{
    auto it = extraLegIds_.find(leg);
    if (it != extraLegIds_.end())
        return it->second;
    return Ip2Nic::getLocalIdOfLeg(leg);
}

MacNodeId MlIp2Nic::getNextHopNodeId(const inet::Ipv4Address& destAddr, int leg, MacNodeId sourceId)
{
    if (nodeType_ == NODEB) {
        // resolve the destination to the UE leg id served by THIS station
        MacNodeId destId = getMlBinder()->getUeIdServedBy(destAddr, nodeId_);
        if (destId != NODEID_NONE)
            return destId;
        return Ip2Nic::getNextHopNodeId(destAddr, leg, sourceId);
    }

    // UE: an extra-leg packet goes to that leg's serving node (no D2D on extra legs)
    if (leg >= 2)
        return binder_->getServingNodeOrSelf(getLocalIdOfLeg(leg));

    return Ip2Nic::getNextHopNodeId(destAddr, leg, sourceId);
}

} // namespace simu5g
