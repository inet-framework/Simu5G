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

#ifndef _PDCP_DOWNLINK_RELAY_H_
#define _PDCP_DOWNLINK_RELAY_H_

#include "simu5g/stack/pdcp/PdcpTxEntityBase.h"

namespace simu5g {

/**
 * @class PdcpDownlinkRelay
 * @brief Downlink half of a DC-secondary PDCP relay (see PdcpRelayEntity).
 *
 * Receives already-processed PDCP PDUs (from the master via X2) and forwards
 * them directly to RLC without any PDCP processing (no compression,
 * no PDCP header, no sequence numbering).
 */
class PdcpDownlinkRelay : public PdcpTxEntityBase
{
    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    static omnetpp::simsignal_t pdcpSduSentSignal_;

  public:
    void initialize(int stage) override;
    void handlePacketFromUpperLayer(inet::Packet *pkt) override;
};

} // namespace simu5g

#endif
