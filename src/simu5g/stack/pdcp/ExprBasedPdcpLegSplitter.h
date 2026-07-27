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

#ifndef _EXPR_BASED_PDCP_LEG_SPLITTER_H_
#define _EXPR_BASED_PDCP_LEG_SPLITTER_H_

#include <inet/common/ModuleRefByPar.h>
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/binder/Binder.h"

namespace simu5g {

/**
 * @class ExprBasedPdcpLegSplitter
 * @brief Expression-driven TX-side leg dispatcher of a multi-leg PdcpEntity compound.
 *
 * Evaluates the legSelectionRule expression per PDU for the leg index and
 * applies the leg's id mapping (see ExprBasedPdcpLegSplitter.ned).
 */
class ExprBasedPdcpLegSplitter : public omnetpp::cSimpleModule
{
  protected:
    // Provides the variable bindings for the legSelectionRule expression
    class LegResolver : public cDynamicExpression::IResolver {
        ExprBasedPdcpLegSplitter *module_;
      public:
        LegResolver(ExprBasedPdcpLegSplitter *module) : module_(module) {}
        IResolver *dup() const override { return new LegResolver(module_); }
        cValue readVariable(cExpression::Context *context, const char *name) override;
    };

    static omnetpp::simsignal_t sentPacketToLowerLayerSignal_;
    static omnetpp::simsignal_t pdcpSduSentSignal_;
    static omnetpp::simsignal_t pdcpSduSentNrSignal_;

    inet::ModuleRefByPar<Binder> binder_;

    int numLegs_ = 1;
    bool isUe_ = false;                  // selects leg 1's id mapping: UE's NR stack vs a master's X2 leg

    MacNodeId nodeId_ = NODEID_NONE;     // this node's (LTE/base) id
    MacNodeId nrNodeId_ = NODEID_NONE;   // this UE's NR-leg id (UEs only)

    // Leg selection rule (from the NED parameter)
    cDynamicExpression *legSelectionRule_ = nullptr;

    // Current evaluation context (set before each evaluation)
    bool currentUseNR_ = false;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;

  public:
    ~ExprBasedPdcpLegSplitter() override;
};

} // namespace simu5g

#endif
