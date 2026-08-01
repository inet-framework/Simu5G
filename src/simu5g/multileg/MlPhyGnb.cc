//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#include "simu5g/multileg/MlPhyGnb.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(MlPhyGnb);

int MlPhyGnb::getReceiverGateIndex(const cModule *receiver, MacNodeId dest) const
{
    int leg = check_and_cast<const MlBinder *>(binder_.get())->getLegOfNode(dest);
    if (leg >= 2) {
        std::string gateName = "nrRadioIn" + std::to_string(leg);
        int gate = receiver->findGate(gateName.c_str());
        if (gate < 0)
            throw cRuntimeError("receiver %s has no gate '%s' for leg %d",
                    receiver->getFullPath().c_str(), gateName.c_str(), leg);
        return gate;
    }
    return LtePhyEnbD2D::getReceiverGateIndex(receiver, dest);
}

} // namespace simu5g
