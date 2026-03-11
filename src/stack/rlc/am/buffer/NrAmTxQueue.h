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

#ifndef __SIMU5G_NRAMTXQUEUE_H_
#define __SIMU5G_NRAMTXQUEUE_H_

#include <omnetpp.h>
#include "AmTxQueue.h"

#include "stack/mac/LteMacBase.h"
#include "RlcSduSlidingWindowTransmissionBuffer.h"
#include "RlcSduRetransmissionBuffer.h"
#include "stack/rlc/am/packet/LteRlcAmSdu_m.h"

using namespace omnetpp;

namespace simu5g {


class NrAmTxQueue : public cSimpleModule
{
protected:

    struct SDUInfo {
        inet::Packet *sdu;
        int currentOffset;
        SDUInfo() {
            sdu = nullptr;
            currentOffset=0;
        }
        ~SDUInfo() {
            if (sdu) {
                delete sdu;
            }
        }
    };

    std::list<SDUInfo*> sduBuffer;


    RlcSduSlidingWindowTransmissionBuffer* txBuffer;
    std::list<omnetpp::cPacket*> controlBuffer;
    RlcSduRetransmissionBuffer* rtxBuffer;

    //Used to prevent sending messages after a RLF.
    bool radioLinkFailureDetected;


    /*
     * Reference to the corresponding RLC AM module
     */
    inet::ModuleRefByPar<LteRlcAm> lteRlc_;

    /*
     * Copy of LTE control info - used for sending down PDUs and control packets.
     */

    FlowControlInfo *lteInfo_ = nullptr;
    /*
     * Copy of MacCid corresponding to lteInfo_ - used to report buffer status
     *
     */
    MacCid infoCid_;

    // TODO: Workaroung. We need the mac to avoid getting stalled and indicate available data
    LteMacBase *mac ;

    //Debug
    std::string name_entity;

    //Maximum PDU SN transmitted so far
    unsigned int sn;
    unsigned int tx_next_ack;
    unsigned int AM_window_size;
    unsigned int pdu_without_poll;
    unsigned int byte_without_poll;
    unsigned int pollPDU;
    unsigned int pollByte;
    unsigned int poll_sn;
    unsigned int maxRtxThreshold;
    bool setPoll;
    cMessage* t_PollRetransmitTimer;
    simtime_t t_PollRetransmit;

    static simsignal_t wastedGrantedBytes;
    static simsignal_t enqueuedSduSize;
    static simsignal_t enqueuedSduRate;
    static simsignal_t requestedPduSize;
    static simsignal_t txWindowOccupation;
    static simsignal_t txWindowFull;
    static simsignal_t retransmissionPdu;
    simtime_t lastSduSample;
    unsigned int sduSample;
    unsigned int receivedSdus;

    void initialize() override;
    void finish() override;
    void handleMessage(cMessage *msg) override;
    virtual bool sendRetransmission(int size);
    virtual void reportBufferStatus();
    virtual bool checkPolling();
    virtual void sendSegment(PendingSegment segment);
public:
    virtual ~NrAmTxQueue();

    /*
     * Enqueues an upper layer packet into the transmission buffer
     * @param sdu the packet to be enqueued
     */
    virtual void enque(Packet *sdu) ;

    virtual void sendPdus(int size) ;
    virtual void handleControlPacket(omnetpp::cPacket *pkt) ;
    virtual void bufferControlPdu(omnetpp::cPacket *pkt) ;
    virtual unsigned int getPendingDataVolume() const;
};

} //namespace

#endif
