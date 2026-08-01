//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include "simu5g/multileg/MlBinder.h"
#include "simu5g/multileg/MlBearerManagement.h"

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

std::vector<int> MlBinder::getFrerLegsOf(MacNodeId ueId)
{
    if (ueId == NODEID_NONE || getNodeTypeById(ueId) != UE)
        return {};
    cModule *rrc = getRrcByNodeId(ueId);
    auto *bm = rrc ? dynamic_cast<MlBearerManagement *>(rrc->getSubmodule("bearerManagement")) : nullptr;
    return bm ? bm->getFrerLegs() : std::vector<int>();
}

void MlBinder::establishDataConnection(FlowControlInfo *info)
{
    MacNodeId srcId = info->getSourceId();
    MacNodeId destId = info->getDestId();
    bool isMulticast = info->getMulticastGroupId() != NODEID_NONE;

    bool ueIsSource = (getNodeTypeById(srcId) == UE);
    MacNodeId ueId = ueIsSource ? srcId : (!isMulticast && getNodeTypeById(destId) == UE ? destId : NODEID_NONE);

    std::vector<int> frerLegs = getFrerLegsOf(ueId);
    if (frerLegs.empty()) {
        Binder::establishDataConnection(info);
        return;
    }

    // one bearer per replica leg, at both endpoints
    for (int leg : frerLegs) {
        MacNodeId ueLegId = getPeerLegId(ueId, leg);
        if (ueLegId == NODEID_NONE)
            continue;
        FlowControlInfo legInfo = *info;
        if (ueIsSource)
            legInfo.setSourceId(ueLegId);
        else
            legInfo.setDestId(ueLegId);
        createConnection(&legInfo, true);
    }
}

MacNodeId MlBinder::getPeerLegId(MacNodeId anyLegId, int leg)
{
    cModule *node = getNodeModule(anyLegId);
    if (node == nullptr)
        return NODEID_NONE;
    if (leg == LEG_LTE)
        return node->hasPar("macNodeId") ? MacNodeId(node->par("macNodeId").intValue()) : NODEID_NONE;
    std::string parName = (leg == LEG_NR) ? "nrMacNodeId" : "nrMacNodeId" + std::to_string(leg);
    return node->hasPar(parName.c_str()) ? MacNodeId(node->par(parName.c_str()).intValue()) : NODEID_NONE;
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
