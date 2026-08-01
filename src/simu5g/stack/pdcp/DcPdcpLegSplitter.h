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

#ifndef _DC_PDCP_LEG_SPLITTER_H_
#define _DC_PDCP_LEG_SPLITTER_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/stack/pdcp/PdcpLegSplitter.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

/**
 * @brief TX-side leg dispatcher for a dual-connectivity split bearer.
 *
 * Adds the stock LTE/NR pair's id mapping and statistics to the generic
 * PdcpLegSplitter mechanism (see DcPdcpLegSplitter.ned).
 */
class DcPdcpLegSplitter : public PdcpLegSplitter
{
  protected:
    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    static omnetpp::simsignal_t pdcpSduSentSignal_;
    static omnetpp::simsignal_t pdcpSduSentNrSignal_;

    inet::ModuleRefByPar<Binder> binder_;

    bool isUe_ = false;                  // selects leg 1's id mapping: UE's NR stack vs a master's X2 leg

    MacNodeId nodeId_ = NODEID_NONE;     // this node's (LTE/base) id
    MacNodeId nrNodeId_ = NODEID_NONE;   // this UE's NR-leg id (UEs only)

    void initialize(int stage) override;
    void processForLeg(inet::Packet *pkt, int leg) override;
};

} // namespace simu5g

#endif
