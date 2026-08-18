//
//                  Simu5G
//
// Authors: Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _DC_PDCP_LEG_SPLITTER_H_
#define _DC_PDCP_LEG_SPLITTER_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include "simu5g/common/binder/Binder.h"
#include "simu5g/stack/ip2nic/HandoverPacketHolderUe.h"

namespace simu5g {

/**
 * @brief TX-side leg dispatcher for a dual-connectivity split bearer.
 *
 * Chooses a leg for each PDU from the legSelection policy expression, and
 * applies that leg's DC-specific id mapping (see DcPdcpLegSplitter.ned).
 */
class DcPdcpLegSplitter : public omnetpp::cSimpleModule
{
  protected:
    // Variable bindings for the legSelection expression
    class PolicyResolver : public omnetpp::cDynamicExpression::IResolver {
        DcPdcpLegSplitter *module_;
      public:
        PolicyResolver(DcPdcpLegSplitter *module) : module_(module) {}
        IResolver *dup() const override { return new PolicyResolver(module_); }
        omnetpp::cValue readVariable(omnetpp::cExpression::Context *context, const char *name) override;
    };

    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    static omnetpp::simsignal_t pdcpSduSentSignal_;
    static omnetpp::simsignal_t pdcpSduSentNrSignal_;

    inet::ModuleRefByPar<Binder> binder_;

    int numLegs_ = 1;
    bool isUe_ = false;                  // selects leg 1's id mapping: UE's NR stack vs a master's X2 leg

    MacNodeId nodeId_ = NODEID_NONE;     // this node's (LTE/base) id
    MacNodeId nrNodeId_ = NODEID_NONE;   // this UE's NR-leg id (UEs only)

    // UEs only: the latched serving node ids this module reads (see isLegLive())
    omnetpp::opp_component_ptr<HandoverPacketHolderUe> handoverPacketHolder_;

    omnetpp::cDynamicExpression *legSelection_ = nullptr;

    // Evaluation context of the policy expression, set before each evaluation
    int currentTypeOfService_ = 0;
    int currentPacketOrdinal_ = 0;

    // PDUs this bearer has steered by evaluating the policy; the "packetOrdinal" variable
    int packetsSteered_ = 0;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;

    // Which leg carries this PDU. A leg whose peer is not attached is not offered to the
    // policy: with one leg live the choice is made for us, and the policy only decides
    // between legs that can actually carry the PDU.
    virtual int selectLeg(const FlowControlInfo *lteInfo);
    virtual bool isLegLive(int leg, const FlowControlInfo *lteInfo);

  public:
    ~DcPdcpLegSplitter() override;
};

} // namespace simu5g

#endif
