//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

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
        // resolve the extra legs' modules from the nrMac<k>Module/nrRlcMux<k>Module parameters
        for (int leg = 2; ; leg++) {
            std::string macPar = "nrMac" + std::to_string(leg) + "Module";
            std::string muxPar = "nrRlcMux" + std::to_string(leg) + "Module";
            if (!hasPar(macPar.c_str()) || par(macPar.c_str()).stdstringValue().empty())
                break;
            extraLegMacs_[leg] = inet::getModuleFromPar<LteMacBase>(par(macPar.c_str()), this);
            extraLegRlcMuxes_[leg] = inet::getModuleFromPar<RlcMux>(par(muxPar.c_str()), this);
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
    if (leg >= 2)
        return getMlRegistration()->getNodeIdOfLeg(leg);
    return BearerManagement::getLocalIdOfLeg(leg);
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
    if (leg >= 2)
        return extraLegRlcMuxes_.at(leg);
    return BearerManagement::getRlcMux(leg);
}

LteMacBase *MlBearerManagement::getMac(int leg)
{
    if (leg >= 2)
        return extraLegMacs_.at(leg);
    return BearerManagement::getMac(leg);
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
