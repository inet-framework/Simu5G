#ifndef _D2D_MODE_CONTROLLER_H_
#define _D2D_MODE_CONTROLLER_H_

#include <map>
#include <set>

#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

class RlcUmTxEntity;

class D2DModeController : public cSimpleModule
{
  protected:
    typedef std::map<MacNodeId, std::set<RlcUmTxEntity *>> PerPeerTxEntities;
    PerPeerTxEntities perPeerTxEntities_;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }

  public:
    void registerD2DPeerTxEntity(MacNodeId peerId, RlcUmTxEntity *umTxEnt);
    void resumeDownstreamInPackets(MacNodeId peerId);
    bool isEmptyingTxBuffer(MacNodeId peerId);
};

} // namespace simu5g

#endif
