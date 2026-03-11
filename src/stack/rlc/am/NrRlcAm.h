//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __SIMU5G_NRRLCAM_H_
#define __SIMU5G_NRRLCAM_H_

#include <omnetpp.h>
#include "LteRlcAm.h"
#include "stack/rlc/am/buffer/NrAmTxQueue.h"
#include "stack/rlc/am/buffer/NrAmRxQueue.h"

using namespace omnetpp;

namespace simu5g {
class NrAmRxQueue;
class NrAmTxQueue;
/**
 * TODO - Generated class
 */
class NrRlcAm : public LteRlcAm
{
protected:

    /*
     * Data structures
     */

    typedef std::map<MacCid, NrAmTxQueue *> NrAmTxBuffers;
    typedef std::map<MacCid, NrAmRxQueue *> NrAmRxBuffers;
    NrAmTxBuffers txBuffers;
    NrAmRxBuffers rxBuffers;

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
    NrAmTxQueue *getNrTxBuffer(MacNodeId nodeId, LogicalCid lcid) ;

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
    NrAmRxQueue *getNrRxBuffer(MacNodeId nodeId, LogicalCid lcid);
    void handleUpperMessage(cPacket *pktAux) override;

    void handleLowerMessage(cPacket *pktAux) override;
public:
    virtual void deleteQueues(MacNodeId nodeId) override;
    /**
     * bufferControlPdu() is invoked by the RXBuffer as a direct method
     * call and is used to forward control packets to be sent down upon
     * the next MAC request.
     *
     * @param pkt packet to buffer
     */
    void bufferControlPdu(cPacket *pkt) override;

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

      virtual void routeControlMessage(cPacket *pkt) override;

};

} //namespace

#endif
