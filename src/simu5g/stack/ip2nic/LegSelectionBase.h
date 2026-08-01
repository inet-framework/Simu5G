#ifndef __LEGSELECTIONBASE_H_
#define __LEGSELECTIONBASE_H_

#include <omnetpp.h>
#include <unordered_map>
#include <inet/common/InitStages.h>
#include <inet/common/packet/Packet.h>
#include <inet/networklayer/contract/ipv4/Ipv4Address.h>
#include "simu5g/common/LteCommon.h"

namespace simu5g {

using namespace omnetpp;

/**
 * @brief Base class for leg selection modules (ILegSelection implementations).
 *
 * Provides the packet-path plumbing: it reads the IP header of each packet,
 * asks the subclass's selectLeg() for the leg index, attaches the choice as a
 * LegReq tag, and forwards the packet. A selectLeg() return of DROP_PACKET
 * discards the packet (e.g. when no leg is attached).
 *
 * It also provides the machinery for expression-based policies: an expression
 * resolver exposing the typeOfService and packetOrdinal variables, and the
 * per-flow packet counter behind the latter (the counter only advances when
 * computePacketOrdinal() is called, i.e. when a policy is actually evaluated).
 */
class LegSelectionBase : public cSimpleModule
{
  protected:
    // selectLeg() return value: discard the packet instead of forwarding it
    static constexpr int DROP_PACKET = -1;

    // Represents a flow using source address, destination address, and type of service.
    struct FlowKey {
        uint32_t srcAddr;
        uint32_t dstAddr;
        uint16_t typeOfService;
        bool operator==(const FlowKey& other) const { return srcAddr == other.srcAddr && dstAddr == other.dstAddr && typeOfService == other.typeOfService; }
    };

    // Hash function for FlowKey
    struct FlowKeyHash {
        std::size_t operator()(const FlowKey& key) const {
            return std::hash<uint32_t>()(key.srcAddr) ^ (std::hash<uint32_t>()(key.dstAddr) << 1) ^ (std::hash<uint16_t>()(key.typeOfService) << 2);
        }
    };

    // Custom resolver that provides variable bindings for the policy expressions
    class PolicyResolver : public cDynamicExpression::IResolver {
        LegSelectionBase *module_;
      public:
        PolicyResolver(LegSelectionBase *module) : module_(module) {}
        IResolver *dup() const override { return new PolicyResolver(module_); }
        cValue readVariable(cExpression::Context *context, const char *name) override;
    };

    cGate *lowerLayerOut_ = nullptr;

    RanNodeType nodeType_;      // UE or NODEB

    // Per-flow packet counter (for split bearer alternation via packetOrdinal)
    std::unordered_map<FlowKey, int, FlowKeyHash> splitBearersTable_;

    // Current evaluation context (set before each evaluate() call)
    int currentTypeOfService_ = 0;
    int currentPacketOrdinal_ = 0;

    void initialize(int stage) override;
    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void handleMessage(cMessage *msg) override;

    // Duplicate a NED expr() parameter and bind it to this module's PolicyResolver.
    // The caller owns (deletes) the returned expression.
    cDynamicExpression *makePolicyExpression(cPar& par);

    // Advance and latch the per-flow packet ordinal for the given flow; call
    // before evaluating a policy expression that may use packetOrdinal.
    void computePacketOrdinal(inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService);

    // The leg the packet should be sent on (its LegReq tag value), or
    // DROP_PACKET to discard the packet.
    virtual int selectLeg(inet::Packet *pkt, inet::Ipv4Address srcAddr, inet::Ipv4Address destAddr, int typeOfService) = 0;
};

} //namespace

#endif
