#ifndef __LTENRLEGSELECTION_H_
#define __LTENRLEGSELECTION_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/stack/ip2nic/LegSelectionBase.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief Leg selection for the stock LTE/NR leg pair (0 = LTE, 1 = NR).
 *
 * Selects per packet: a leg whose stack is not attached to a serving node is
 * never chosen (packets are dropped when neither is); when both legs are
 * available, the choice is delegated to the dcLegPolicy/legPolicy expression
 * parameters (see LteNrLegSelection.ned).
 */
class LteNrLegSelection : public LegSelectionBase
{
  protected:
    // reference to the binder
    inet::ModuleRefByPar<Binder> binder_;

    // LTE MAC node id of this node
    MacNodeId nodeId_ = NODEID_NONE;
    // NR MAC node id of this node (if enabled)
    MacNodeId nrNodeId_ = NODEID_NONE;

    // Enable for dual connectivity
    bool dualConnectivityEnabled_;

    // Policy expressions (from NED parameters)
    cDynamicExpression *dcLegPolicy_ = nullptr;
    cDynamicExpression *legPolicy_ = nullptr;

    void initialize(int stage) override;
    int selectLeg(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService) override;

  public:
    ~LteNrLegSelection() override;
};

} //namespace

#endif
