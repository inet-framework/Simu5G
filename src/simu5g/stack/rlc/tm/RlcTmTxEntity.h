#ifndef _SIMU5G_RLCTMTXENTITY_H_
#define _SIMU5G_RLCTMTXENTITY_H_

#include <inet/common/packet/Packet.h>

#include "simu5g/stack/rlc/RlcTxEntityBase.h"

namespace simu5g {

/**
 * @brief Transmission entity for Transparent Mode (TM).
 *
 * Buffers SDUs from the upper layer without segmentation.
 * Sends queued PDUs to MAC upon request.
 */
class RlcTmTxEntity : public RlcTxEntityBase
{
  protected:
    inet::cPacketQueue queuedPdus_;
    int queueSize_ = 0;   // max queue length in packets (0: unlimited)

    static inet::simsignal_t rlcPacketLossDlSignal_;
    static inet::simsignal_t rlcPacketLossUlSignal_;

    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

    void handleSdu(inet::Packet *pkt);
    void handleMacSduRequest(inet::Packet *pkt);
};

} // namespace simu5g

#endif
