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

    // The RLC configuration of the bearer this entity serves, as RRC records it in the
    // bearer's descriptor (see DrbTable).
    //
    // soFraming selects the wire format: LTE FI with concatenation (TS 36.322), or NR
    // SI/SO with one SDU/segment per PDU, which lets the MAC multiplex several PDUs into
    // one grant (TS 38.322). That multiplexing path is implemented for UM only, so the NR
    // AM entity keeps the base FI answer here even though its own PDUs carry SO headers.
    virtual bool usesSoFraming() const { return false; }
    virtual unsigned int snFieldLength() const { return 12; }

    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
};

} // namespace simu5g

#endif
