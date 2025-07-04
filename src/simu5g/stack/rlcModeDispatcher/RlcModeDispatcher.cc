//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/rlcModeDispatcher/RlcModeDispatcher.h"

namespace simu5g {

Define_Module(RlcModeDispatcher);

using namespace omnetpp;
using namespace inet;

void RlcModeDispatcher::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        // Initialize gate references
        pdcpInGate_ = gate("pdcpIn");

        // Read RLC type configuration parameters
        conversationalRlc_ = aToRlcType(par("conversationalRlc"));
        interactiveRlc_ = aToRlcType(par("interactiveRlc"));
        streamingRlc_ = aToRlcType(par("streamingRlc"));
        backgroundRlc_ = aToRlcType(par("backgroundRlc"));

        EV << "RlcModeDispatcher::initialize - RLC type configuration:" << endl;
        EV << "  Conversational: " << rlcTypeToA(conversationalRlc_) << endl;
        EV << "  Interactive: " << rlcTypeToA(interactiveRlc_) << endl;
        EV << "  Streaming: " << rlcTypeToA(streamingRlc_) << endl;
        EV << "  Background: " << rlcTypeToA(backgroundRlc_) << endl;
    }
}

void RlcModeDispatcher::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *arrivalGate = pkt->getArrivalGate();

    EV << "RlcModeDispatcher::handleMessage - Received packet " << pkt->getName()
       << " from " << arrivalGate->getName() << endl;

    if (arrivalGate == pdcpInGate_) {
        // Packet from PDCP going down to RLC
        auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
        setTrafficInformation(pkt, lteInfo, (Direction)lteInfo->getDirection());
        dispatchToRlc(pkt);
    } else {
        // Packet from RLC going up to PDCP
        send(pkt, "pdcpOut");
    }
}

LteTrafficClass RlcModeDispatcher::classifyTraffic(cPacket *pkt)
{
    // Traffic classification based on packet name (application type)
    // This logic is extracted from the original PDCP implementation

    if ((strcmp(pkt->getName(), "VoIP")) == 0) {
        return CONVERSATIONAL;
    }
    else if ((strcmp(pkt->getName(), "gaming")) == 0) {
        return INTERACTIVE;
    }
    else if ((strcmp(pkt->getName(), "VoDPacket") == 0) ||
             (strcmp(pkt->getName(), "VoDFinishPacket") == 0)) {
        return STREAMING;
    }
    else {
        return BACKGROUND;
    }
}

LteRlcType RlcModeDispatcher::determineRlcType(LteTrafficClass trafficClass)
{
    switch (trafficClass) {
        case CONVERSATIONAL:
            return conversationalRlc_;
        case INTERACTIVE:
            return interactiveRlc_;
        case STREAMING:
            return streamingRlc_;
        case BACKGROUND:
            return backgroundRlc_;
        default:
            return backgroundRlc_;  // Default fallback
    }
}

void RlcModeDispatcher::setTrafficInformation(cPacket *pkt, inet::Ptr<FlowControlInfo> lteInfo, Direction direction)
{
    // Classify the traffic
    LteTrafficClass trafficClass = classifyTraffic(pkt);

    // Determine RLC type based on traffic class
    LteRlcType rlcType = determineRlcType(trafficClass);

    // Set application type based on traffic class
    ApplicationType appType;
    switch (trafficClass) {
        case CONVERSATIONAL:
            appType = VOIP;
            break;
        case INTERACTIVE:
            appType = GAMING;
            break;
        case STREAMING:
            appType = VOD;
            break;
        case BACKGROUND:
        default:
            appType = CBR;
            break;
    }

    // Update the control info
    lteInfo->setApplication(appType);
    lteInfo->setTraffic(trafficClass);
    lteInfo->setRlcType(rlcType);
    lteInfo->setDirection(direction);

    EV << "RlcModeDispatcher::setTrafficInformation - Packet: " << pkt->getName()
       << ", Traffic: " << lteTrafficClassToA(trafficClass)
       << ", RLC: " << rlcTypeToA(rlcType) << endl;
}

void RlcModeDispatcher::dispatchToRlc(inet::Packet *pkt)
{
    auto lteInfo = pkt->getTag<FlowControlInfo>();

    // Determine gate name based on RLC type
    const char *gateName;
    switch (lteInfo->getRlcType()) {
        case UM: gateName = "UM_Sap$o"; break;
        case AM: gateName = "AM_Sap$o"; break;
        case TM: gateName = "TM_Sap$o"; break;
        default: throw cRuntimeError("RlcModeDispatcher::dispatchToRlc(): invalid RlcType %d", lteInfo->getRlcType());
    }

    // Determine gate index based on NR usage and gate vector size
    int preferredIndex = lteInfo->getUseNR() ? 1 : 0;
    int gateVectorSize = gateSize(gateName);
    int gateIndex;

    if (preferredIndex < gateVectorSize) {
        // Preferred index is available
        gateIndex = preferredIndex;
    } else if (gateVectorSize > 0) {
        // Preferred index not available, fall back to index 0
        gateIndex = 0;
        EV << "RlcModeDispatcher::dispatchToRlc - Warning: Requested "
           << (lteInfo->getUseNR() ? "NR" : "LTE") << " gate index " << preferredIndex
           << " not available (vector size: " << gateVectorSize
           << "), falling back to index 0" << endl;
    } else {
        throw cRuntimeError("RlcModeDispatcher::dispatchToRlc(): no gates available for %s", gateName);
    }

    EV << "RlcModeDispatcher::dispatchToRlc - Sending packet " << pkt->getName()
       << " with RLC type " << rlcTypeToA((LteRlcType)lteInfo->getRlcType())
       << " to " << (lteInfo->getUseNR() ? "NR" : "LTE") << " RLC (index " << gateIndex
       << " of " << gateVectorSize << ")" << endl;

    send(pkt, gateName, gateIndex);
}

} //namespace
