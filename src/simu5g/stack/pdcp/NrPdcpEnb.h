//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _NRPDCPRRCENB_H_
#define _NRPDCPRRCENB_H_

#include <inet/common/ModuleRefByPar.h>

#include "simu5g/common/LteDefs.h"
#include "simu5g/stack/pdcp/NrTxPdcpEntity.h"
#include "simu5g/stack/pdcp/LtePdcpEnbD2D.h"
#include "simu5g/stack/dualConnectivityManager/DualConnectivityManager.h"

namespace simu5g {

/**
 * @class NrPdcpEnb
 * @brief PDCP Layer
 *
 * This is the PDCP/RRC layer of NR Stack
 *
 */
class NrPdcpEnb : public LtePdcpEnbD2D
{

  protected:

    // Flag for enabling Dual Connectivity
    bool dualConnectivityEnabled_;

    // Reference to the Dual Connectivity Manager
    inet::ModuleRefByPar<DualConnectivityManager> dualConnectivityManager_;

    void initialize(int stage) override;

    /**
     * Handler for data port
     * @param pkt Incoming packet
     */
    void fromDataPort(cPacket *pktAux) override;

    /**
     * Handler for um/am sap
     *
     * It performs the following steps:
     * - Decompresses the header, restoring the original packet
     * - Decapsulates the packet
     * - Sends the packet to the application layer
     *
     * @param pkt Incoming packet
     */
    void fromLowerLayer(cPacket *pkt) override;

    MacNodeId getDestId(const Ipv4Address& destAddr, bool useNR, MacNodeId sourceId) override;

    /**
     * getEntity() is used to gather the NR PDCP entity
     * for that LCID. If the entity was already present, a reference
     * is returned; otherwise, a new entity is created,
     * added to the entities map and a reference is returned as well.
     *
     * @param lcid Logical CID
     * @return Pointer to the PDCP entity for the LCID of the flow
     *
     */
    LteTxPdcpEntity *getTxEntity(MacCid lcid) override;
    LteRxPdcpEntity *getRxEntity(MacCid cid) override;

    /*
     * Dual Connectivity support
     */
    bool isDualConnectivityEnabled() override { return dualConnectivityEnabled_; }

    // Send packet to the target node by invoking the Dual Connectivity manager
    void forwardDataToTargetNode(Packet *pkt, MacNodeId targetNode) override;

    // Receive packet from the source node. Called by the Dual Connectivity manager
    void receiveDataFromSourceNode(Packet *pkt, MacNodeId sourceNode) override;

  private:
    int drbIndex;

  public:
    virtual void activeUeUL(std::set<MacNodeId> *ueSet);

};

} //namespace

#endif

