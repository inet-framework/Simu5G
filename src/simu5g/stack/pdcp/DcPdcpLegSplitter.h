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
#include "simu5g/stack/rrc/DrbDesc.h"    // SPLIT_THRESHOLD_INFINITY

namespace simu5g {

class RlcTxEntityBase;

/**
 * @brief TX-side leg dispatcher for a dual-connectivity split bearer.
 *
 * Chooses a leg for each PDU -- by the primaryPath/threshold buffer-occupancy
 * policy, or a legSelection override -- and applies that leg's DC-specific id
 * mapping (see DcPdcpLegSplitter.ned).
 */
class DcPdcpLegSplitter : public omnetpp::cSimpleModule
{
  protected:
    // Variable bindings for the legSelection override expression
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
    std::vector<CellGroup> legGroups_;   // cell group of each leg, in leg-index order (the legs parameter)
    bool isUe_ = false;                  // selects the SCG leg's id mapping: UE's secondary stack vs a master's X2 leg

    MacNodeId nodeId_ = NODEID_NONE;     // this node's (LTE/base) id
    MacNodeId nrNodeId_ = NODEID_NONE;   // this UE's NR-leg id (UEs only)

    // UEs only: mirror of RRC's stack attachment ledger, pushed at creation and on
    // every handover event (see setServingNodeIds()); NODEID_NONE = not attached
    MacNodeId servingNodeId_ = NODEID_NONE;     // the LTE stack's serving node
    MacNodeId nrServingNodeId_ = NODEID_NONE;   // the NR stack's serving node

    // The split policy (TS 38.331 PDCP-Config), pushed by RRC (see setSplitConfig()):
    // the bearer rides primaryPath_ until the pending data volume reaches splitThreshold_.
    CellGroup primaryPath_ = MCG;
    int64_t splitThreshold_ = SPLIT_THRESHOLD_INFINITY;

    // The TX RLC entity of each leg the splitter can weigh, in leg-index order; nullptr
    // where the leg has no local RLC (a DC master's secondary leg, whose queue is across
    // X2). Pushed at establishment (see setLegRlc()).
    std::vector<RlcTxEntityBase *> legRlc_;

    // This splitter's direction-appropriate leg-selection expression (ulLegSelection at a
    // UE, dlLegSelection at a base station; RRC pushes the matching one, see setLegSelection).
    // nullptr = none: uplink falls back to the least-occupied leg, downlink to the primary.
    omnetpp::cDynamicExpression *legSelection_ = nullptr;

    // Evaluation context of the override expression, set before each evaluation
    int currentPacketOrdinal_ = 0;

    // PDUs this bearer has steered by the override; the "packetOrdinal" variable
    int packetsSteered_ = 0;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(omnetpp::cMessage *msg) override;

    // Which leg carries this PDU. A leg whose peer is not attached is not offered to the
    // policy: with one leg live the choice is made for us, and the policy only decides
    // between legs that can actually carry the PDU.
    virtual int selectLeg(const FlowControlInfo *lteInfo);
    virtual bool isLegLive(int leg, const FlowControlInfo *lteInfo);

    // Evaluate the legSelection expression to a live leg index (throws otherwise).
    virtual int applyLegSelection(const std::vector<bool>& live);

    // The leg index whose cell group is the primary path.
    virtual int primaryLeg() const;

  public:
    // Configuration push from RRC (see BearerManagement): the bearer's split policy --
    // the primary path cell group and the ul-DataSplitThreshold in bytes.
    virtual void setSplitConfig(CellGroup primaryPath, int64_t ulDataSplitThreshold);

    // Configuration push from RRC: this bearer's leg-selection expression for this
    // splitter's direction (RRC passes ulLegSelection to a UE, dlLegSelection to a base
    // station), as an "expr(...)" source string. Compiled here, so errors throw at
    // establishment.
    virtual void setLegSelection(const char *spec);

    // Configuration push from RRC: the TX RLC entity a leg was wired to, so the splitter
    // can read its pending data volume for the split decision. UE legs and a master's
    // local leg only; a master's secondary leg has none.
    virtual void setLegRlc(int leg, RlcTxEntityBase *txEntity);

    // RRC's push of the stacks' attachment (see BearerManagement::pushServingNodeIds()):
    // the serving node of the UE's LTE and NR stack, current as of handover start.
    // UEs only; a base station's splitter steers by the Binder.
    virtual void setServingNodeIds(MacNodeId servingNodeId, MacNodeId nrServingNodeId);

    ~DcPdcpLegSplitter() override;
};

} // namespace simu5g

#endif
