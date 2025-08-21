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

#ifndef _RRC_H_
#define _RRC_H_

#include "simu5g/common/LteDefs.h"
#include "simu5g/common/LteControlInfo.h"
#include <inet/common/ModuleRefByPar.h>

// Forward declarations for specific module types
namespace simu5g {
    class LteMacBase;
    class LtePdcpBase;
}

using namespace omnetpp;

namespace simu5g {

/**
 * @brief RRC (Radio Resource Control) module for LTE/NR networks.
 */
class Rrc : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    // Module references for accessing other stack layers
    inet::ModuleRefByPar<LteMacBase> macModule;
    inet::ModuleRefByPar<cModule> rlcModule;  // Compound module with TM/UM/AM submodules
    inet::ModuleRefByPar<LtePdcpBase> pdcpModule;

  public:
    Rrc() {}
    virtual ~Rrc() {}

    // Create a data connection through the MAC layer
    virtual void createDataConnection(MacNodeId sourceId, MacNodeId destId,
                                    LogicalCid lcid, Direction direction,
                                    LteTrafficClass trafficClass, LteRlcType rlcType,
                                    const inet::Ipv4Address& srcAddr,
                                    const inet::Ipv4Address& dstAddr,
                                    uint16_t typeOfService);
};

} // namespace simu5g

#endif
