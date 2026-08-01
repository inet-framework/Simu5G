//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLPHYGNB_H_
#define _MULTILEG_MLPHYGNB_H_

#include "simu5g/stack/phy/LtePhyEnbD2D.h"
#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

/**
 * @brief gNB PHY that can deliver to a UE's extra legs.
 *
 * The stock receive-gate resolution knows the LTE/NR gate pair (radioIn /
 * nrRadioIn); frames for an extra leg's id (index >= 2) must arrive on that
 * leg's own gate (nrRadioIn2 etc.), which this override resolves via the
 * MlBinder's id -> leg map.
 */
class MlPhyGnb : public LtePhyEnbD2D
{
  protected:
    int getReceiverGateIndex(const omnetpp::cModule *receiver, MacNodeId dest) const override;
};

} // namespace simu5g

#endif
