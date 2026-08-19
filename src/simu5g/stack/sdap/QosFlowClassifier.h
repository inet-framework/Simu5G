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

#ifndef _QOS_FLOW_CLASSIFIER_H_
#define _QOS_FLOW_CLASSIFIER_H_

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

#include "simu5g/common/QfiRuleSet.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief Classifies a UE's uplink packets to QoS flows (see QosFlowClassifier.ned).
 *
 * The uplink counterpart of TrafficFlowFilter's QFI assignment: each direction is
 * classified once, at its ingress -- downlink where it enters the tunnel at the
 * core network, uplink here, where it enters the UE's stack. A matched packet is
 * stamped with a QfiReq tag that SDAP consumes; an unmatched one is left
 * untagged, which is what lets reflective QoS (or the default flow) take over.
 */
class QosFlowClassifier : public cSimpleModule
{
  protected:
    QfiRuleSet qfiRules_;

    void initialize() override;
    void handleMessage(cMessage *msg) override;
};

} // namespace simu5g

#endif
