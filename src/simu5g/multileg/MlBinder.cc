//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

Define_Module(MlBinder);

MacNodeId MlBinder::getLegMacNodeId(inet::Ipv4Address address, int leg)
{
    if (leg == LEG_LTE)
        return getMacNodeId(address);
    if (leg == LEG_NR)
        return getNrMacNodeId(address);
    auto legIt = ipAddressToLegMacNodeId_.find(leg);
    if (legIt == ipAddressToLegMacNodeId_.end())
        return NODEID_NONE;
    auto it = legIt->second.find(address);
    return it != legIt->second.end() ? it->second : NODEID_NONE;
}

MacNodeId MlBinder::getUeIdServedBy(inet::Ipv4Address address, MacNodeId bsId)
{
    // stock legs first, then the extra-leg maps
    for (MacNodeId id : { getMacNodeId(address), getNrMacNodeId(address) })
        if (id != NODEID_NONE && getServingNodeOrSelf(id) == bsId)
            return id;
    for (const auto& [leg, map] : ipAddressToLegMacNodeId_) {
        auto it = map.find(address);
        if (it != map.end() && getServingNodeOrSelf(it->second) == bsId)
            return it->second;
    }
    return NODEID_NONE;
}

void MlBinder::setMacNodeId(inet::Ipv4Address address, MacNodeId nodeId)
{
    int leg = getLegOfNode(nodeId);
    if (leg >= 2)
        ipAddressToLegMacNodeId_[leg][address] = nodeId;
    else
        Binder::setMacNodeId(address, nodeId);
}

cModule *MlBinder::getPhyByNodeId(MacNodeId nodeId)
{
    int leg = getLegOfNode(nodeId);
    if (leg < 2)
        return Binder::getPhyByNodeId(nodeId);
    cModule *module = getNodeModule(nodeId);
    if (module == nullptr)
        return nullptr;
    return module->getSubmodule("cellularNic")->getSubmodule(("nrPhy" + std::to_string(leg)).c_str());
}

cModule *MlBinder::getMacByNodeId(MacNodeId nodeId)
{
    int leg = getLegOfNode(nodeId);
    if (leg < 2)
        return Binder::getMacByNodeId(nodeId);
    cModule *module = getNodeModule(nodeId);
    if (module == nullptr)
        return nullptr;
    return module->getSubmodule("cellularNic")->getSubmodule(("nrMac" + std::to_string(leg)).c_str());
}

void MlBinder::unregisterNode(MacNodeId id)
{
    for (auto& [leg, map] : ipAddressToLegMacNodeId_)
        for (auto it = map.begin(); it != map.end(); )
            it = (it->second == id) ? map.erase(it) : std::next(it);
    legOfNodeId_.erase(id);
    Binder::unregisterNode(id);
}

} // namespace simu5g
