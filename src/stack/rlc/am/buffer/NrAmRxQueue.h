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

#ifndef __SIMU5G_NRAMRXQUEUE_H_
#define __SIMU5G_NRAMRXQUEUE_H_

#include <omnetpp.h>
//#include "AmRxQueue.h"
#include "RlcSduSlidingWindowReceptionBuffer.h"
#include "stack/rlc/am/NrRlcAm.h"
#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/Packet.h>
#include "common/timer/TTimer.h"
#include "stack/pdcp_rrc/packet/LtePdcpPdu_m.h"
#include "stack/rlc/LteRlcDefs.h"

#include "stack/rlc/am/packet/LteRlcAmPdu.h"


namespace simu5g {
using namespace omnetpp;



class NrRlcAm;
/**
 * TODO - Generated class
 */
class NrAmRxQueue : public cSimpleModule
{
public:
    virtual ~NrAmRxQueue();

    virtual void enque(inet::Packet *pkt);
    virtual  void handleControlPdu(inet::Packet* pkt);
    virtual void setRemoteEntity(MacNodeId id);


protected:
    // parent RLC AM module
    inet::ModuleRefByPar<NrRlcAm> lteRlc_;

    // Binder module
    inet::ModuleRefByPar<Binder> binder_;
    /*
     * FlowControlInfo matrix: used for CTRL messages generation
     */
    FlowControlInfo *flowControlInfo_ = nullptr;

    //Debug:
        std::string name_entity;


    //Statistics
    static unsigned int totalCellRcvdBytes_;
    unsigned int totalRcvdBytes_;
    Direction dir_ = UNKNOWN_DIRECTION;
    static simsignal_t rlcCellPacketLossSignal_[2];
    static simsignal_t rlcPacketLossSignal_[2];
    static simsignal_t rlcPduPacketLossSignal_[2];
    static simsignal_t rlcDelaySignal_[2];
    static simsignal_t rlcPduDelaySignal_[2];
    static simsignal_t rlcCellThroughputSignal_[2];
    static simsignal_t rlcThroughputSignal_[2];
    static simsignal_t rlcPduThroughputSignal_[2];

    static simsignal_t rlcThroughputSampleSignal_[2];
    static simsignal_t rlcCellThroughputSampleSignal_[2];
    static simsignal_t  rxWindowOccupation;
    static simsignal_t  rxWindowFull;
    cModule* remoteEntity;
    simtime_t lastTputSample;
    unsigned int tpsample;
    simtime_t lastCellTputSample;
    unsigned int cellTpsample;

    //Map for SDU reassembly
    std::map<unsigned int, std::vector<std::pair<unsigned int, unsigned int>>> sduMap;
    cMessage* t_ReassemblyTimer;
    simtime_t t_Reassembly;
    simtime_t t_StatusProhibit;
    cMessage* t_StatusProhibitTimer;
    simtime_t lastSentAck_;
    cMessage* throughputTimer;
    simtime_t throughputInterval;
    RlcSduSlidingWindowReceptionBuffer* pduBuffer;
    //Set of passed up PDU
    std::set<unsigned int> passedUpSdu;
    //! Check if the SDU carried in the index PDU has been
    //! completely received

    unsigned int rx_next_status_trigger;
    unsigned int AM_window_size;

    bool status_report_pending;

    virtual void initialize() override;

    //! Pass an SDU to the upper layer (RLC receiver)
    /** @param <index> index The index of the first PDU related to
     *  the target SDU (i.e. the SDU that has been completely received)
     */
    virtual void passUp(const int index);



    virtual void sendStatusReport();

    virtual void handleMessage(cMessage* msg) override;


};

} //namespace

#endif
