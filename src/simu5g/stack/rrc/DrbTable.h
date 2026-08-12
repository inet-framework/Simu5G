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

using namespace omnetpp;

namespace simu5g {

/**
 * @brief The node's data radio bearer configuration, owned by RRC.
 *
 * Holds one ~DrbDesc per bearer, keyed by (peer node, DRB id). ~BearerManagement fills
 * the table in as it establishes bearers, and drops entries together with the bearer's
 * PDCP entity.
 */
class DrbTable : public cSimpleModule
{
  protected:
    std::map<DrbKey, DrbDesc> drbs_;

  protected:
    void initialize() override;
    void handleMessage(cMessage *msg) override;
    void refreshDisplay() const override;

  public:
    virtual const DrbDesc *findDrb(DrbKey key) const;
    virtual DrbDesc& getOrCreateDrb(DrbKey key);
    virtual const std::map<DrbKey, DrbDesc>& getDrbs() const { return drbs_; }

    // Teardown: mirrors the lifecycle of the bearer's PDCP entity (see
    // BearerManagement::deleteLocalPdcpEntities)
    virtual void removeDrb(DrbKey key);
    virtual void removeDrbsOfPeer(MacNodeId peerId);
    virtual void removeAllDrbs();
};

} // namespace simu5g

#endif
