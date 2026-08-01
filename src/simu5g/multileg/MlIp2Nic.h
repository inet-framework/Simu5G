//
// Multileg demo project for Simu5G. Emulates an EXTERNAL project: nothing
// outside src/simu5g/multileg/ and simulations/multileg/ may be modified.
//
// Authors: Andras Varga (OpenSim Ltd)
//

#ifndef _MULTILEG_MLIP2NIC_H_
#define _MULTILEG_MLIP2NIC_H_

#include "simu5g/stack/ip2nic/Ip2Nic.h"
#include "simu5g/multileg/MlBinder.h"

namespace simu5g {

/**
 * @brief Ip2Nic for nodes aware of extra legs (indices >= 2).
 *
 * At a UE, getLocalIdOfLeg() answers for the extra legs (node parameters
 * nrMacNodeId<k>) and getNextHopNodeId() routes an extra-leg packet to that
 * leg's serving node. At a base station, getNextHopNodeId() resolves the
 * destination address to the UE leg id served by THIS station, so a DL
 * packet reaches the peer over the leg it belongs to.
 */
class MlIp2Nic : public Ip2Nic
{
  protected:
    // extra legs: leg index -> this node's MacNodeId on that leg (UE only)
    std::map<int, MacNodeId> extraLegIds_;

    void initialize(int stage) override;

    MlBinder *getMlBinder() { return omnetpp::check_and_cast<MlBinder *>(binder_.get()); }

    MacNodeId getLocalIdOfLeg(int leg) const override;
    MacNodeId getNextHopNodeId(const inet::Ipv4Address& destAddr, int leg, MacNodeId sourceId) override;
};

} // namespace simu5g

#endif
