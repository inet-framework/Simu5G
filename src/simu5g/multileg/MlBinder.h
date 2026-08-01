//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLBINDER_H_
#define _MULTILEG_MLBINDER_H_

#include "simu5g/common/binder/Binder.h"

namespace simu5g {

/**
 * @brief Binder with per-leg id bookkeeping for nodes with more than two legs.
 *
 * The stock Binder bins a node's ids into two IP-keyed maps (LTE and NR) by
 * the id's range, so a second NR-range id of the same node would clobber the
 * first. This subclass keeps an explicit id -> leg map (fed by
 * MlRegistration) and gives extra legs (index >= 2) their own IP-keyed maps
 * and their own module-name resolution (nrPhy2/nrMac2 etc.).
 */
class MlBinder : public Binder
{
  protected:
    // leg of each extra-leg id (legs 0/1 are implied by the id range and stay
    // in the stock maps)
    std::map<MacNodeId, int> legOfNodeId_;

    // per-extra-leg id maps, keyed like the stock ipAddressTo*MacNodeId_ maps
    std::map<int, std::map<inet::Ipv4Address, MacNodeId>> ipAddressToLegMacNodeId_;

  public:
    // Declare an id to be an extra leg's (called by MlRegistration BEFORE the
    // leg's MAC binds the node's IP address to the id).
    virtual void setLegOfNode(MacNodeId nodeId, int leg) { legOfNodeId_[nodeId] = leg; }

    // The leg of the given id: the declared one, else 0/1 by the id's range.
    virtual int getLegOfNode(MacNodeId nodeId) const {
        auto it = legOfNodeId_.find(nodeId);
        return it != legOfNodeId_.end() ? it->second : (isNrUe(nodeId) ? LEG_NR : LEG_LTE);
    }

    // The node id of the given leg for the given IP address (NODEID_NONE if none).
    virtual MacNodeId getLegMacNodeId(inet::Ipv4Address address, int leg);

    // The UE leg id (any leg) that the given base station serves for the given
    // IP address, or NODEID_NONE. Used by a gNB to resolve a DL destination to
    // the leg it serves.
    virtual MacNodeId getUeIdServedBy(inet::Ipv4Address address, MacNodeId bsId);

    // Extra-leg ids go to their own map; everything else follows the stock binning.
    void setMacNodeId(inet::Ipv4Address address, MacNodeId nodeId) override;

    // Extra-leg ids resolve to their own leg's submodules (nrPhy2/nrMac2 etc.).
    cModule *getPhyByNodeId(MacNodeId nodeId) override;
    cModule *getMacByNodeId(MacNodeId nodeId) override;

    void unregisterNode(MacNodeId id) override;
};

} // namespace simu5g

#endif
