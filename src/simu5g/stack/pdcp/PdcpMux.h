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

#ifndef _PDCP_MUX_H_
#define _PDCP_MUX_H_

#include <map>
#include <set>

#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"

namespace simu5g {

using namespace omnetpp;


/**
 * @brief Upper-layer PDCP packet dispatcher.
 *
 * Owns the TX routing table, which maps each DRB to the index of the
 * toTxEntity gate that serves it. BearerManagement wires the gate and
 * registers the index here. Also collects RX entity output to forward
 * to the upper layer.
 */
class PdcpMux : public cSimpleModule
{
  protected:
    bool isNR_ = false;

    cGate *upperLayerInGate_ = nullptr;
    cGate *upperLayerOutGate_ = nullptr;

    std::map<DrbKey, int> txGateIndices_;  // DRB -> index of the toTxEntity gate serving it

  public:
    bool hasTxEntity(DrbKey id) const { return txGateIndices_.find(id) != txGateIndices_.end(); }
    void registerTxEntity(DrbKey id, int gateIndex);
    void unregisterTxEntity(DrbKey id);

  protected:
    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

    virtual void fromDataPort(cPacket *pkt);
};

} // namespace simu5g

#endif
