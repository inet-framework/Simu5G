#ifndef _D2D_MODE_CONTROLLER_H_
#define _D2D_MODE_CONTROLLER_H_

#include <map>
#include <set>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class ID2dRlcUmTxEntity;

class D2DModeController : public cSimpleModule
{
  protected:
    typedef std::map<MacNodeId, std::set<ID2dRlcUmTxEntity *>> PerPeerTxEntities;
    PerPeerTxEntities perPeerTxEntities_;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

  public:
    virtual void registerD2DPeerTxEntity(MacNodeId peerId, ID2dRlcUmTxEntity *umTxEnt);
    /**
     * Counterpart of registerD2DPeerTxEntity(). This map holds raw pointers to entity
     * *modules*, which BearerManagement::deleteLocalRlcQueues() deletes on handover and on
     * bearer teardown, so every registered entity has to withdraw itself before it goes.
     */
    virtual void unregisterD2DPeerTxEntity(MacNodeId peerId, ID2dRlcUmTxEntity *umTxEnt);
    virtual void resumeDownstreamInPackets(MacNodeId peerId);
    virtual bool isEmptyingTxBuffer(MacNodeId peerId);
};

} // namespace simu5g

#endif
