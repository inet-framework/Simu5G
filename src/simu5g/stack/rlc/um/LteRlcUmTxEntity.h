//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_LTERLCUMTXENTITY_H_
#define _SIMU5G_LTERLCUMTXENTITY_H_

#include "simu5g/stack/rlc/um/RlcUmTxEntityBase.h"
#include "simu5g/mec/utils/MecCommon.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief LTE (TS 36.322) RLC UM transmission entity.
 *
 * FI framing + concatenation of multiple SDUs per PDU, one sequence number per
 * PDU, emits LteRlcUmDataPdu. Uses a bounded SDU queue (drops on overflow).
 */
class LteRlcUmTxEntity : public RlcUmTxEntityBase
{
    struct FragmentInfo {
        inet::Packet *pkt = nullptr;
        int size = 0;
    };
    FragmentInfo *fragmentInfo = nullptr;

    /*
     * @author Alessandro Noferi
     * burst tracking for packetFlowObserver (discarded packets and delay);
     * null-safe via hasListeners().
     */
    RlcBurstStatus burstStatus_;

    // The SDU enqueue buffer.
    inet::cPacketQueue sduQueue_;

    // Whether the first item in the queue is a fragment or a whole SDU.
    bool firstIsFragment_ = false;

    // The maximum available queue size, in bytes (0: unlimited).
    unsigned int queueSize_;

    // The currently stored amount of data in the SDU queue, in bytes.
    unsigned int queueLength_ = 0;

    // Next PDU sequence number to be assigned.
    unsigned int sno_ = 0;

  public:

    ~LteRlcUmTxEntity() override { delete fragmentInfo; }

    // force the sequence number
    void setNextSequenceNumber(unsigned int nextSno) { sno_ = nextSno; }

    // remove the last SDU from the queue
    void removeDataFromQueue();

    void clearQueue() override;
    void resumeDownstreamInPackets() override;
    void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer) override;

  protected:

    void initMode() override;
    bool storeSdu(inet::Packet *pkt) override;
    void rlcPduMake(int pduSize) override;
    bool usesSoFraming() const override { return false; }
    // Unused by the LTE FI/concatenation path; kept at the historical default so
    // the FlowControlInfo stamped by the base is byte-identical to the pre-split entity.
    unsigned int snFieldLength() const override { return 12; }

  private:

    // enqueue an upper-layer SDU into the TX buffer; false if the queue is full
    bool enque(inet::cPacket *pkt);
};

} //namespace

#endif
