#ifndef _SIMU5G_RLCTMRXENTITY_H_
#define _SIMU5G_RLCTMRXENTITY_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/rlc/RlcRxEntityBase.h"

namespace simu5g {

/**
 * @brief Receiver entity for Transparent Mode (TM).
 *
 * Simply forwards received PDUs to the upper layer without
 * reassembly or reordering.
 */
class RlcTmRxEntity : public RlcRxEntityBase
{
  protected:
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;
};

} // namespace simu5g

#endif
