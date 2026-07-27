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

#ifndef _PDCP_UPLINK_RELAY_H_
#define _PDCP_UPLINK_RELAY_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/stack/pdcp/PdcpRxEntityBase.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

/**
 * @class PdcpUplinkRelay
 * @brief Uplink half of a DC-secondary PDCP relay (see PdcpRelayEntity).
 *
 * Receives PDCP PDUs from RLC (UL from the UE) and forwards them
 * directly to the master node via X2 without any PDCP processing
 * (no decompression, no PDCP header removal, no reordering).
 */
class PdcpUplinkRelay : public PdcpRxEntityBase
{
  protected:
    inet::ModuleRefByPar<Binder> binder_;
    MacNodeId nodeId_ = NODEID_NONE;

  public:
    void initialize(int stage) override;
    void handlePacketFromLowerLayer(inet::Packet *pkt) override;
};

} // namespace simu5g

#endif
