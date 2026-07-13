//
//                  Simu5G
//
// Authors: Esteban Egea Lopez (Universidad Politecnica de Cartagena), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _SIMU5G_NRRLCUMTXENTITY_H_
#define _SIMU5G_NRRLCUMTXENTITY_H_

#include "simu5g/stack/rlc/um/RlcUmTxEntityBase.h"
#include "simu5g/stack/rlc/um/RlcUmTransmitterBuffer.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief NR (TS 38.322) RLC UM transmission entity.
 *
 * SI + byte-offset (SO) segmentation, one SDU segment per PDU, one sequence
 * number per SDU, emits NrRlcUmDataPdu. Uses an unbounded SDU segmentation
 * buffer (RlcUmTransmitterBuffer).
 */
class NrRlcUmTxEntity : public RlcUmTxEntityBase
{
    // The SDU segmentation buffer.
    RlcUmTransmitterBuffer *sduBuffer = nullptr;

    // UM SN field length in bits (TS 38.322: 6 or 12).
    unsigned int sn_FieldLength = 12;

  public:

    ~NrRlcUmTxEntity() override
    {
        if (sduBuffer) {
            sduBuffer->clearBuffer();
            delete sduBuffer;
        }
    }

    void clearQueue() override;
    void resumeDownstreamInPackets() override;
    bool isTxBufferEmpty() const override { return !sduBuffer->hasData(); }
    void rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer) override;

  protected:

    void initMode() override;
    bool storeSdu(inet::Packet *pkt) override;
    void rlcPduMake(int pduSize) override;
    bool usesSoFraming() const override { return true; }
    unsigned int snFieldLength() const override { return sn_FieldLength; }

  private:

    // Once the old-mode entity has drained, release the new-mode entity's holding
    // buffer via the D2D controller (mode-switch handover of buffered SDUs).
};

} //namespace

#endif
