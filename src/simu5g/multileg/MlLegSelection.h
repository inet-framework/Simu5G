//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLLEGSELECTION_H_
#define _MULTILEG_MLLEGSELECTION_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/stack/ip2nic/LegSelectionBase.h"
#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

/**
 * @brief Leg selection for a UE with extra NR legs (indices >= 2).
 *
 * A leg is available when its stack is attached to a serving node (looked up
 * in the binder per packet). When more than one leg is available, the
 * legPolicy expression picks one; an unavailable pick falls back to the
 * first available leg. At a base station, the leg of the UE id this station
 * serves is chosen.
 */
class MlLegSelection : public LegSelectionBase
{
  protected:
    inet::ModuleRefByPar<MlBinder> binder_;

    // this node's MacNodeId per leg (UE only; NODEID_NONE = leg absent)
    std::vector<MacNodeId> legNodeIds_;

    MacNodeId nodeId_ = NODEID_NONE;   // base stations: own id

    cDynamicExpression *legPolicy_ = nullptr;

    void initialize(int stage) override;
    int selectLeg(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService) override;

  public:
    ~MlLegSelection() override;
};

} // namespace simu5g

#endif
