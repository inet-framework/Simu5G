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

#ifndef _LTE_LTERLCAM_H_
#define _LTE_LTERLCAM_H_

#include <omnetpp.h>
#include "common/LteCommon.h"
#include "common/LteControlInfo.h"
#include "stack/rlc/LteRlcDefs_m.h"
namespace simu5g {

using namespace omnetpp;

class AmTxQueue;
class AmRxQueue;

/**
 * @class LteRlcAm
 * @brief AM Module
 *
 * This is the AM Module of RLC.
 * It implements the acknowledged mode (AM):
 *
 * TODO
 */
class LteRlcAm : public cSimpleModule
{
  protected:

    static simsignal_t radioLinkFailureSignal;
    static simsignal_t receivedPacketFromUpperLayerSignal_;
    static simsignal_t receivedPacketFromLowerLayerSignal_;
    static simsignal_t sentPacketToUpperLayerSignal_;
    static simsignal_t sentPacketToLowerLayerSignal_;
    /*
     * Data structures
     */

    typedef std::map<MacCid, AmTxQueue *> AmTxBuffers;
    typedef std::map<MacCid, AmRxQueue *> AmRxBuffers;

    /**
     * The buffers map associates each CID with
     * a TX/RX Buffer, identified by its ID
     */

    AmTxBuffers txBuffers_;
    AmRxBuffers rxBuffers_;

    cGate *upInGate_ = nullptr;
    cGate *upOutGate_ = nullptr;
    cGate *downInGate_ = nullptr;
    cGate *downOutGate_ = nullptr;

  protected:

    /**
     * Analyze gate of incoming packet
     * and call proper handler
     */
    virtual void handleMessage(cMessage *msg) override;

    virtual void initialize() override;
    virtual void finish() override
    {
    }



    /**
     * getTxBuffer() is used by the sender to gather the TXBuffer
     * for that CID. If TXBuffer was already present, a reference
     * is returned, otherwise a new TXBuffer is created,
     * added to the tx_buffers map and a reference is returned as well.
     *
     * @param lcid Logical Connection ID
     * @param nodeId MAC Node Id
     * @return pointer to the TXBuffer for that CID
     *
     */

    AmTxQueue *getTxBuffer(MacNodeId nodeId, LogicalCid lcid);

    /**
     * getRxBuffer() is used by the receiver to gather the RXBuffer
     * for that CID. If RXBuffer was already present, a reference
     * is returned, otherwise a new RXBuffer is created,
     * added to the rx_buffers map and a reference is returned as well.
     *
     * @param lcid Logical Connection ID
     * @param nodeId MAC Node Id
     * @return pointer to the RXBuffer for that CID
     *
     */

    virtual AmRxQueue *getRxBuffer(MacNodeId nodeId, LogicalCid lcid);

    /**
     * handler for traffic coming
     * from the upper layer (PDCP)
     *
     * handleUpperMessage() performs the following tasks:
     * - Adds the RLC-Am header to the packet, containing
     *   the CID, the Traffic Type and the Sequence Number
     *   of the packet (extracted from the IP Datagram)
     * - Searches (or adds) the proper TXBuffer, depending
     *   on the packet CID
     * - Calls the TXBuffer, that from now on takes
     *   care of the packet
     *
     * @param pkt packet to process
     */
    virtual void handleUpperMessage(cPacket *pkt);

    /**
     * Am Mode
     *
     * handler for traffic coming from
     * lower layer (DTCH, MTCH, MCCH).
     *
     * handleLowerMessage() performs the following task:
     *
     * - Searches (or adds) the proper RXBuffer, depending
     *   on the packet CID
     * - Calls the RXBuffer, that from now on takes
     *   care of the packet
     *
     * @param pkt packet to process
     */
    virtual void handleLowerMessage(cPacket *pkt);

  public:
    // add deleteQueues here to be able to delete during handover
    /**
     * deleteQueues() must be called on handover
     * to delete queues for a given user
     *
     * @param nodeId Id of the node whose queues are deleted
     */
    virtual void deleteQueues(MacNodeId nodeId);
    /**
     * handler for control messages coming
     * from receiver AM entities
     *
     * routeControlMessage() performs the following task:
     * - Searches the proper TXBuffer, depending
     *   on the packet CID and delivers the control packet to it
     *
     * @param pkt packet to process
     */
    virtual void routeControlMessage(cPacket *pkt);

    /**
     * sendFragmented() is invoked by the TXBuffer as a direct method
     * call and is used to forward fragments to lower layers. This is needed
     * since the TXBuffer itself has no output gates
     *
     * @param pkt packet to forward
     */
    void sendFragmented(cPacket *pkt);

    /**
     * bufferControlPdu() is invoked by the RXBuffer as a direct method
     * call and is used to forward control packets to be sent down upon
     * the next MAC request.
     *
     * @param pkt packet to buffer
     */
    virtual void bufferControlPdu(cPacket *pkt);

    /**
     * sendDefragmented() is invoked by the RXBuffer as a direct method
     * call and is used to forward fragments to upper layers. This is needed
     * since the RXBuffer itself has no output gates
     *
     * @param pkt packet to forward
     */
    void sendDefragmented(cPacket *pkt);

    /**
     * informMacOfWaitingData() sends a new data notification to the MAC
     */
    void indicateNewDataToMac(cPacket *pkt);
};

} //namespace

#endif

