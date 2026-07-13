//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _NRMACGNBD2D_H_
#define _NRMACGNBD2D_H_

#include "simu5g/stack/mac/NrMacGnb.h"
#include "simu5g/stack/mac/buffer/LteMacBuffer.h"
#include "simu5g/stack/mac/buffer/harq_d2d/LteHarqBufferMirrorD2D.h"
#include "simu5g/stack/rrc/D2DModeSwitchNotification_m.h"
#include "simu5g/stack/mac/conflict_graph/ConflictGraph.h"
#include "simu5g/stack/d2d/mac/ID2dMacEnb.h"
#include "simu5g/stack/d2d/mac/D2dEnbMacHelper.h"
#include <inet/common/ModuleRefByPar.h>

namespace simu5g {

using namespace omnetpp;

class ConflictGraph;

/**
 * NR-gNB MAC with D2D support.
 *
 * This is the NR counterpart of ~LteMacEnbD2D, and its sibling implementation:
 * the D2D-specific state and heavy logic already live in the shared
 * D2dEnbMacHelper, so the two leaves only duplicate the thin dispatch/glue
 * overrides. Keep the two in sync.
 */
class NrMacGnbD2D : public NrMacGnb, public ID2dMacEnb
{
  protected:
    // holds the D2D-specific eNB-MAC state and logic (shared with the LTE variant)
    D2dEnbMacHelper d2dEnbHelper_;

    /**
     * macPduUnmake() extracts SDUs from a received MAC
     * PDU and sends them to the upper layer.
     *
     * On ENB it also extracts the BSR Control Element
     * and stores it in the BSR buffer (for the cid from
     * which the packet was received)
     *
     * @param pkt container packet
     */
    void macPduUnmake(cPacket *pkt) override;

    void macHandleFeedbackPkt(cPacket *pkt) override;
    /**
     * creates scheduling grants (one for each nodeId) according to the Schedule List.
     * It sends them to the lower layer
     */
    void sendGrants(std::map<GHz, LteMacScheduleList> *scheduleList) override;

    /**
     * Flush Tx H-ARQ buffers for all users
     */
    void flushHarqBuffers() override;

    /// Lower Layer Handler
    void fromPhy(cPacket *pkt) override;

    /// HARQ RX buffer factory: adds support for the D2D and D2D_MULTI directions
    LteHarqBufferRx *createRxHarqBuffer(MacNodeId src, const UserControlInfo *userInfo) override;

  public:

    NrMacGnbD2D();

    /**
     * Reads MAC parameters for ue and performs initialization.
     */
    void initialize(int stage) override;

    /**
     * Main loop
     */
    void handleSelfMessage() override;

    void handleMessage(cMessage *msg) override;

    bool isReuseD2DEnabled() override
    {
        return d2dEnbHelper_.getReuseD2D();
    }

    bool isReuseD2DMultiEnabled() override
    {
        return d2dEnbHelper_.getReuseD2DMulti();
    }

    ConflictGraph *getConflictGraph() override
    {
        return d2dEnbHelper_.getConflictGraph();
    }

    void deleteHarqBuffersMirrorD2D(MacNodeId txPeer, MacNodeId rxPeer) override;

    /**
     * deleteQueues() on ENB performs actions
     * from base classes and also deletes mirror buffers
     *
     * @param nodeId id of node performing handover
     */
    void deleteQueues(MacNodeId nodeId) override;

    // get the reference to the "mirror" buffers
    HarqBuffersMirrorD2D *getHarqBuffersMirrorD2D(GHz carrierFrequency) override;

    // delete the "mirror" Harq Buffer for this node (useful at handover)
    void deleteHarqBuffersMirrorD2D(MacNodeId nodeId);

    // send the D2D Mode Switch signal to the transmitter of the given flow
    void sendModeSwitchNotification(MacNodeId srcId, MacNodeId dst, LteD2DMode oldMode, LteD2DMode newMode) override;

    bool isMsHarqInterrupt() override { return d2dEnbHelper_.getMsHarqInterrupt(); }

};

} //namespace

#endif

