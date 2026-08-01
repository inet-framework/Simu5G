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

#include <inet/common/InitStages.h>
#include <inet/common/packet/Packet.h>
#include "simu5g/common/LteCommon.h"

namespace simu5g {

/**
 * @brief Generic TX-side leg dispatcher for a multi-leg PDCP entity.
 *
 * Pure mechanism: sends each PDU on the leg its LegReq tag names, falling back
 * to leg 0 when the selected leg does not exist or is torn down. Makes no
 * assumption about what technology a leg is; per-leg processing (id
 * adaptation, statistics) is left to the processForLeg() hook, which does
 * nothing here (see DcPdcpLegSplitter for the stock LTE/NR DC mapping).
 */
class PdcpLegSplitter : public omnetpp::cSimpleModule
{
  protected:
    int numLegs_ = 1;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;

    // Guard: the selected leg, or the fallback (leg 0) when the selected leg
    // does not exist or its gate is unconnected (leg torn down).
    virtual int checkLegAvailable(int leg);

    // Per-leg processing before the PDU leaves on out[leg]: id adaptation,
    // statistics. The base does nothing (the PDU's ids are left as they are).
    virtual void processForLeg(inet::Packet *pkt, int leg) {}
};

} // namespace simu5g

#endif
