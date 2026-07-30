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

#ifndef _PDCP_LEG_JOINER_H_
#define _PDCP_LEG_JOINER_H_

#include <omnetpp.h>

namespace simu5g {

/**
 * @class PdcpLegJoiner
 * @brief RX-side leg merger of a multi-leg PdcpEntityBase compound.
 *
 * Relays all legs' PDU streams into the single RX entity; the RX entity's
 * reorder window restores SN order across legs.
 */
class PdcpLegJoiner : public omnetpp::cSimpleModule
{
  protected:
    void handleMessage(omnetpp::cMessage *msg) override;
};

} // namespace simu5g

#endif
