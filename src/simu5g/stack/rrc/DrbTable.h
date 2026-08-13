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

#ifndef _DRB_TABLE_H_
#define _DRB_TABLE_H_

#include <map>

#include "simu5g/stack/rrc/DrbDesc.h"

namespace omnetpp { class cValueArray; }

using namespace omnetpp;

namespace simu5g {

/**
 * @brief The node's data radio bearer configuration, owned by RRC.
 *
 * Holds two collections of ~DrbDesc records, both keyed by (peer node, DRB id):
 *
 * - The AUTHORED configuration, loaded from the drbConfig parameter: the bearers this
 *   node is configured with, before and independent of their establishment. On the UE
 *   side entries are keyed by NODEID_NONE ("my serving node"); on the gNB side by the
 *   UE's node id. These entries persist for the whole simulation; RRC pushes the SDAP
 *   half of each into the SDAP module (see BearerManagement).
 *
 * - The ESTABLISHED bearers: ~BearerManagement fills this in as it establishes bearers,
 *   and drops entries together with the bearer's PDCP entity.
 */
class DrbTable : public cSimpleModule
{
  protected:
    std::map<DrbKey, DrbDesc> drbs_;            // established bearers
    std::map<DrbKey, DrbDesc> configuredDrbs_;  // authored configuration (drbConfig param)

  protected:
    void initialize() override;
    void handleMessage(cMessage *msg) override;
    void refreshDisplay() const override;

    // Fills configuredDrbs_ from the drbConfig JSON array; if no entry of a peer is marked
    // isDefault, the first one becomes the default DRB.
    virtual void loadConfig(const cValueArray *arr);

  public:
    virtual const DrbDesc *findDrb(DrbKey key) const;
    virtual DrbDesc& getOrCreateDrb(DrbKey key);
    virtual const std::map<DrbKey, DrbDesc>& getDrbs() const { return drbs_; }

    // Authored configuration (UE side: key by NODEID_NONE)
    virtual const DrbDesc *findConfiguredDrb(DrbKey key) const;
    virtual const std::map<DrbKey, DrbDesc>& getConfiguredDrbs() const { return configuredDrbs_; }

    // Teardown: mirrors the lifecycle of the bearer's PDCP entity (see
    // BearerManagement::deleteLocalPdcpEntities). Established bearers only; the
    // authored configuration is not affected.
    virtual void removeDrb(DrbKey key);
    virtual void removeDrbsOfPeer(MacNodeId peerId);
    virtual void removeAllDrbs();
};

} // namespace simu5g

#endif
