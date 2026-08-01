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

#include "simu5g/stack/pdcp/PdcpLegSplitter.h"

#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(PdcpLegSplitter);

void PdcpLegSplitter::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL)
        numLegs_ = par("numLegs");
}

void PdcpLegSplitter::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);

    // Leg choice: follow the LegReq tag -- the per-packet leg decision is owned by
    // the legSelection submodule; this module only executes it.
    int leg = pkt->getTag<LegReq>()->getLeg();
    leg = checkLegAvailable(leg);

    processForLeg(pkt, leg);

    send(pkt, "out", leg);
}

int PdcpLegSplitter::checkLegAvailable(int leg)
{
    if (leg >= numLegs_ || !gate("out", leg)->isConnected()) {
        EV_WARN << NOW << " " << getComponentType()->getName() << " - leg " << leg
                << " is not available (torn down?); falling back to leg 0" << endl;
        leg = 0;
    }
    return leg;
}

} // namespace simu5g
