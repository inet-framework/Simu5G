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

#ifndef _PDCP_LEG_SPLITTER_H_
#define _PDCP_LEG_SPLITTER_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

/**
 * @class PdcpLegSplitter
 * @brief TX-side leg dispatcher of a multi-leg PdcpEntity compound.
 *
 * Picks the RLC leg for each PDCP PDU and applies the leg's id mapping
 * (see PdcpLegSplitter.ned for the full description).
 */
class PdcpLegSplitter : public omnetpp::cSimpleModule
{
  protected:
    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    static omnetpp::simsignal_t pdcpSduSentSignal_;
    static omnetpp::simsignal_t pdcpSduSentNrSignal_;

    inet::ModuleRefByPar<Binder> binder_;

    int numLegs_ = 1;
    std::vector<std::string> legRats_;   // per-leg "rat" field from the legs descriptor

    MacNodeId nodeId_ = NODEID_NONE;     // this node's (LTE/base) id
    MacNodeId nrNodeId_ = NODEID_NONE;   // this UE's NR-leg id (UEs only)

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;
};

} // namespace simu5g

#endif
