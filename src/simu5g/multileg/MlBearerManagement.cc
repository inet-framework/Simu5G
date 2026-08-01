//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include <algorithm>
#include <inet/common/ModuleAccess.h>
#include "simu5g/multileg/MlBearerManagement.h"
#include "simu5g/stack/rlc/RlcMux.h"
#include "simu5g/stack/mac/LteMacBase.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlBearerManagement);

void MlBearerManagement::initialize(int stage)
{
    BearerManagement::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        // FRER: the stack legs every bearer is replicated over (e.g. "1 2 3")
        std::string frerLegs = par("frerLegs").stdstringValue();
        for (auto& tok : cStringTokenizer(frerLegs.c_str()).asIntVector())
            frerLegs_.push_back(tok);

        // Resolve the extra legs' modules from the nrMac<k>Module/nrRlcMux<k>Module
        // parameters. Only a UE has extra stack legs: at a gNB the replicas of a FRER
        // bearer are separate RLC entities over the station's single stack, so its
        // leg lookups stay on the stock modules.
        for (int leg = 2; isUe(); leg++) {
            std::string macPar = "nrMac" + std::to_string(leg) + "Module";
            std::string muxPar = "nrRlcMux" + std::to_string(leg) + "Module";
            if (!hasPar(macPar.c_str()) || par(macPar.c_str()).stdstringValue().empty())
                break;
            auto *mac = inet::findModuleFromPar<LteMacBase>(par(macPar.c_str()), this);
            if (mac == nullptr)
                break;
            extraLegMacs_[leg] = mac;
            extraLegRlcMuxes_[leg] = inet::findModuleFromPar<RlcMux>(par(muxPar.c_str()), this);
        }
    }
}

int MlBearerManagement::legOfLocalId(MacNodeId localNodeId)
{
    int leg = getMlBinder()->getLegOfNode(localNodeId);
    return leg >= 2 ? leg : BearerManagement::legOfLocalId(localNodeId);
}

MacNodeId MlBearerManagement::getLocalIdOfLeg(int leg)
{
    auto *registration = getMlRegistration();
    if (leg >= 2 && registration != nullptr)
        return registration->getNodeIdOfLeg(leg);
    return BearerManagement::getLocalIdOfLeg(leg);
}

bool MlBearerManagement::isLocalNodeId(MacNodeId nodeId)
{
    if (BearerManagement::isLocalNodeId(nodeId))
        return true;
    // the extra legs' own ids count as local too
    auto *registration = getMlRegistration();
    for (int leg = 2; registration != nullptr; leg++) {
        MacNodeId legId = registration->getNodeIdOfLeg(leg);
        if (legId == NODEID_NONE)
            break;
        if (legId == nodeId)
            return true;
    }
    return false;
}

int MlBearerManagement::legOfBearer(FlowControlInfo *lteInfo)
{
    // an extra-leg id on either end marks the bearer's leg; otherwise stock
    int srcLeg = getMlBinder()->getLegOfNode(lteInfo->getSourceId());
    if (srcLeg >= 2)
        return srcLeg;
    int destLeg = getMlBinder()->getLegOfNode(lteInfo->getDestId());
    if (destLeg >= 2)
        return destLeg;
    return BearerManagement::legOfBearer(lteInfo);
}

RlcMux *MlBearerManagement::getRlcMux(int leg)
{
    auto it = extraLegRlcMuxes_.find(leg);
    if (it != extraLegRlcMuxes_.end())
        return it->second;
    return BearerManagement::getRlcMux(leg);
}

LteMacBase *MlBearerManagement::getMac(int leg)
{
    auto it = extraLegMacs_.find(leg);
    if (it != extraLegMacs_.end())
        return it->second;
    return BearerManagement::getMac(leg);
}

MacNodeId MlBearerManagement::getPeerIdOnLeg(MacNodeId peerId, int leg)
{
    // At a UE all legs face the same gNB; at a gNB the peer is the UE's id on that leg.
    if (registration_->getNodeType() == UE)
        return peerId;
    MacNodeId legId = getMlBinder()->getPeerLegId(peerId, leg);
    return legId != NODEID_NONE ? legId : peerId;
}

int MlBearerManagement::getNumLegs(DrbKey id, FlowControlInfo *lteInfo)
{
    if (!frerLegs_.empty() && lteInfo->getMulticastGroupId() == NODEID_NONE)
        return frerLegs_.size();
    return BearerManagement::getNumLegs(id, lteInfo);
}

int MlBearerManagement::selectPdcpLeg(int leg, MacNodeId peerId, DrbKey& compoundId)
{
    if (frerLegs_.empty())
        return BearerManagement::selectPdcpLeg(leg, peerId, compoundId);

    // Which replica is this establishment for? At a UE the stack leg says it; at a
    // gNB the peer id's leg does.
    int stackLeg = (registration_->getNodeType() == UE) ? leg : getMlBinder()->getLegOfNode(peerId);
    auto it = std::find(frerLegs_.begin(), frerLegs_.end(), stackLeg);
    if (it == frerLegs_.end())
        return BearerManagement::selectPdcpLeg(leg, peerId, compoundId);
    int legIdx = it - frerLegs_.begin();

    // All replicas share ONE PDCP entity, keyed by the anchor (first) leg's peer id.
    compoundId = DrbKey(getPeerIdOnLeg(compoundId.getNodeId(), frerLegs_.front()), compoundId.getDrbId());
    return legIdx;
}

void MlBearerManagement::setRlcEntityParams(cModule *entity, int leg)
{
    if (leg < 2) {
        BearerManagement::setRlcEntityParams(entity, leg);
        return;
    }
    // extra NR leg: the leg's own MAC and RLC mux (paths relative to the entity; ^ = the NIC)
    if (entity->hasPar("macModule"))
        entity->par("macModule").setStringValue("^.nrMac" + std::to_string(leg));
    if (entity->hasPar("rlcMuxModule"))
        entity->par("rlcMuxModule").setStringValue("^.nrRlcMux" + std::to_string(leg));
}

} // namespace simu5g
