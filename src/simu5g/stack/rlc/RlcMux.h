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

#ifndef _RLC_MUX_H_
#define _RLC_MUX_H_

#include <map>
#include <set>
#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/mec/utils/MecCommon.h"

namespace simu5g {

using namespace omnetpp;

class BearerManagement;

/**
 * @brief Lower-layer RLC packet dispatcher.
 *
 * Owns the RX routing table, which maps each DRB to the index of the
 * toRxEntity gate that serves it. BearerManagement wires the gate and
 * registers the index here.
 */
class RlcMux : public cSimpleModule
{
  protected:
    static simsignal_t receivedPacketFromLowerLayerSignal_;
    static simsignal_t sentPacketToLowerLayerSignal_;

    BearerManagement *bearerManagement_ = nullptr;

    bool hasD2DSupport_ = false;

    cGate *macInGate_ = nullptr;
    cGate *macOutGate_ = nullptr;

    std::map<DrbKey, int> rxGateIndices_;  // DRB -> index of the toRxEntity gate serving it

    typedef std::map<MacNodeId, Throughput> ULThroughputPerUE;
    ULThroughputPerUE ulThroughput_;

  public:
    virtual void registerRxEntity(DrbKey id, int gateIndex);
    virtual void unregisterRxEntity(DrbKey id);
    virtual void activeUeUL(std::set<MacNodeId> *ueSet);

    virtual void addUeThroughput(MacNodeId nodeId, Throughput throughput);
    virtual double getUeThroughput(MacNodeId nodeId);
    virtual void resetThroughputStats(MacNodeId nodeId);

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

    virtual void fromMacLayer(cPacket *pkt);
};

} // namespace simu5g

#endif
