//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLREGISTRATION_H_
#define _MULTILEG_MLREGISTRATION_H_

#include "simu5g/stack/rrc/Registration.h"
#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

/**
 * @brief Registration for a UE with extra NR legs (leg indices >= 2).
 *
 * On top of the stock LTE/NR pair, announces each extra leg's MacNodeId
 * (node parameter nrMacNodeId<k>) and serving node (nrServingNodeId<k>) to
 * the MlBinder, and declares the id's leg so the binder's IP-keyed
 * bookkeeping does not clobber leg 1's entries.
 */
class MlRegistration : public Registration
{
  protected:
    // extra legs: leg index -> this node's MacNodeId on that leg
    std::map<int, MacNodeId> extraLegNodeIds_;

    void initialize(int stage) override;
    void registerNodes() override;
    void registerServingNodes() override;
    void registerMulticastGroups() override;
    void finish() override;

    MlBinder *getMlBinder() { return omnetpp::check_and_cast<MlBinder *>(binder.get()); }

  public:
    virtual MacNodeId getNodeIdOfLeg(int leg) const;
};

} // namespace simu5g

#endif
