//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _RLC_TX_ENTITY_BASE_H_
#define _RLC_TX_ENTITY_BASE_H_

#include <omnetpp.h>
#include <inet/common/InitStages.h>

#include "simu5g/common/LteControlInfo.h"

namespace simu5g {

/**
 * @brief Abstract base class for all RLC TX entities (UM, TM, AM).
 *
 * Defines the minimal interface that the RlcMux uses to manage
 * TX entities regardless of their RLC mode.
 */
class RlcTxEntityBase : public omnetpp::cSimpleModule
{
  protected:
    FlowControlInfo *flowControlInfo_ = nullptr;

    void handleMessage(omnetpp::cMessage *msg) override;

    ~RlcTxEntityBase() override { delete flowControlInfo_; }

  public:
    virtual void setFlowControlInfo(FlowControlInfo *info);
    FlowControlInfo *getFlowControlInfo() { return flowControlInfo_; }

    // sn-FieldLength (TS 38.331 RLC-BearerConfig), in bits: the RLC entity's own source
    // of truth, read by RRC into the bearer's descriptor (see DrbTable) at establishment.
    virtual unsigned int snFieldLength() const { return 12; }

    // Bytes of SDU data pending initial transmission (not counting AM retransmissions):
    // the RLC data volume a split bearer's leg splitter weighs against the split
    // threshold (see ~DcPdcpLegSplitter, TS 38.331 ul-DataSplitThreshold). The concrete
    // TX entities report it from their own queue; the base has none.
    virtual int64_t getBufferOccupancy() const { return 0; }

    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
};

} // namespace simu5g

#endif
