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

#include "simu5g/stack/pdcp/PdcpLegJoiner.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(PdcpLegJoiner);

void PdcpLegJoiner::handleMessage(cMessage *msg)
{
    EV << "PdcpLegJoiner - merging PDU from leg " << msg->getArrivalGate()->getIndex() << " into the RX entity" << endl;
    send(msg, "out");
}

} // namespace simu5g
