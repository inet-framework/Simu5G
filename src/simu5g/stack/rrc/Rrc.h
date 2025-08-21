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

using namespace omnetpp;

namespace simu5g {

class LteMacBase;
class LtePdcpBase;
class LteRlcUm;

/**
 * @brief RRC (Radio Resource Control) module for LTE/NR networks.
 */
class Rrc : public cSimpleModule
{
  private:
    inet::ModuleRefByPar<LteMacBase> macModule;
    inet::ModuleRefByPar<LteRlcUm> rlcUmModule;  // Compound module with TM/UM/AM submodules
    inet::ModuleRefByPar<LtePdcpBase> pdcpModule;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

  public:
    virtual void createIncomingConnection(FlowControlInfo *lteInfo);
    virtual void createOutgoingConnection(FlowControlInfo *lteInfo);
};

} // namespace simu5g

#endif
