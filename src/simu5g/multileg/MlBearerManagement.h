//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLBEARERMANAGEMENT_H_
#define _MULTILEG_MLBEARERMANAGEMENT_H_

#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/multileg/MlBinder.h"
#include "simu5g/multileg/MlRegistration.h"

namespace simu5g {

/**
 * @brief BearerManagement for a UE NIC with extra NR legs (indices >= 2).
 *
 * Overrides only the leg lookups: the leg of an id/bearer comes from the
 * MlBinder's id -> leg map instead of the id-range check, and each extra
 * leg's MAC/RLC-mux modules come from the nrMac<k>Module/nrRlcMux<k>Module
 * parameters. Bearer establishment itself is inherited unchanged.
 */
class MlBearerManagement : public BearerManagement
{
  protected:
    // extra legs: leg index -> that leg's modules
    std::map<int, RlcMux *> extraLegRlcMuxes_;
    std::map<int, LteMacBase *> extraLegMacs_;

    void initialize(int stage) override;

    MlBinder *getMlBinder() { return omnetpp::check_and_cast<MlBinder *>(binderModule.get()); }
    // nullptr at a node without extra legs (e.g. a gNB, which keeps stock Registration)
    MlRegistration *getMlRegistration() { return dynamic_cast<MlRegistration *>(registration_); }
    bool isUe() { return registration_->getNodeType() == UE; }

    // FRER: replicate every bearer over these stack legs (empty = plain multi-leg mode)
    std::vector<int> frerLegs_;

    int legOfLocalId(MacNodeId localNodeId) override;
    MacNodeId getLocalIdOfLeg(int leg) override;
    bool isLocalNodeId(MacNodeId nodeId) override;
    int legOfBearer(FlowControlInfo *lteInfo) override;
    RlcMux *getRlcMux(int leg) override;
    LteMacBase *getMac(int leg) override;
    void setRlcEntityParams(omnetpp::cModule *entity, int leg) override;

    // FRER bearer layout: one PDCP entity with one leg per replica, anchored at
    // the first FRER leg's key.
    int getNumLegs(DrbKey id, FlowControlInfo *lteInfo) override;
    int selectPdcpLeg(int leg, MacNodeId peerId, DrbKey& compoundId /*inout*/) override;

    // The peer's node id on the given leg (the same gNB for a UE's legs).
    virtual MacNodeId getPeerIdOnLeg(MacNodeId peerId, int leg);

  public:
    // The stack legs every bearer of this node is replicated over (empty = none).
    // MlBinder reads this to establish each replica at both endpoints.
    virtual const std::vector<int>& getFrerLegs() const { return frerLegs_; }
};

} // namespace simu5g

#endif
