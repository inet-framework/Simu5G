//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork), Andras Varga (OpenSim Ltd)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//


#include "NrSdap.h"

#include "simu5g/stack/sdap/packet/NrSdapHeader_m.h"
#include "simu5g/common/QfiTag_m.h"
#include "simu5g/common/LteControlInfoTags_m.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include <inet/common/packet/Packet.h>
#include <inet/common/stlutils.h>
#include <inet/common/ProtocolTag_m.h>

namespace simu5g {

Define_Module(NrSdap);


void NrSdap::initialize()
{
    smf_.reference(this, "smfModule", true);

    // Get pointer to reflective QoS table
    reflectiveQosTable.reference(this, "reflectiveQosTableModule", false);

    // Only UE uses reflective QoS table
    std::string nodeRole = par("nodeRole").stdstringValue();
    isUe = (nodeRole == "UE");
    establishBearersOnDemand_ = par("establishBearersOnDemand").boolValue();
    reflectiveQosOverridesQfi_ = par("reflectiveQosOverridesQfi").boolValue();
    if (!isUe && reflectiveQosTable.getNullable() != nullptr)
        throw cRuntimeError("Only UE may use a reflective QoS table");
}

void NrSdap::configureDrb(const DrbDesc& drb)
{
    Enter_Method("configureDrb(drb %d)", (int)num(drb.getDrbId()));
    EV << "NrSdap::configureDrb - " << drb << endl;
    drbTable_.addOrUpdateDrb(drb);
}

void NrSdap::bearerEstablished(DrbKey key)
{
    Enter_Method_Silent("bearerEstablished()");
    EV << "NrSdap::bearerEstablished - " << key << endl;
    establishedBearers_.insert(key);
}

void NrSdap::bearerReleased(DrbKey key)
{
    Enter_Method_Silent("bearerReleased()");
    EV << "NrSdap::bearerReleased - " << key << endl;
    establishedBearers_.erase(key);
}

bool NrSdap::requiresSdapHeader(const DrbDesc *drb)
{
    // SDAP header is needed when the QFI cannot be unambiguously determined
    // from the DRB alone on the RX side:
    // - default DRB: may carry packets with unmapped QFIs (fallback traffic)
    // - multiple QFIs mapped: reverse mapping is ambiguous
    // Caller must ensure drb is not null.
    return drb->isDefault || drb->qfiList.size() > 1;
}

bool NrSdap::shouldEnableReflectiveQos(Qfi qfi)
{
    return par("useReflectiveQos").boolValue(); // for now -- should come from RRC config
}

const inet::Protocol *NrSdap::getUpperProtocol(const DrbDesc *ctx)
{
    // If an explicit upperProtocol is configured on this DRB, use it
    if (ctx && !ctx->upperProtocol.empty()) {
        const inet::Protocol *proto = inet::Protocol::findProtocol(ctx->upperProtocol.c_str());
        if (!proto)
            throw cRuntimeError("Unknown protocol '%s' in the DRB's upperProtocol configuration", ctx->upperProtocol.c_str());
        return proto;
    }

    // Otherwise derive from pduSessionType
    PduSessionType pduSessionType = ctx ? ctx->pduSessionType : IP_V4;
    switch (pduSessionType) {
        case IP_V4:
        case IP_V4V6:
            return &inet::Protocol::ipv4;
        case IP_V6:
            return &inet::Protocol::ipv6;
        case ETHERNET:
            return &inet::Protocol::ethernetMac;
        case UNSTRUCTURED:
            throw cRuntimeError("Unstructured PDU session requires explicit 'upperProtocol' in the DRB configuration");
        default:
            throw cRuntimeError("Unknown PDU session type: %d", (int)pduSessionType);
    }
}

void NrSdap::handleMessage(cMessage *msg)
{
    auto arrivalGate = msg->getArrivalGate();
    auto pkt = check_and_cast<inet::Packet *>(msg);

    if (arrivalGate == gate("upperLayerIn"))
        handleUpperPacket(pkt);
    else if (arrivalGate == gate("pdcpIn"))
       handleLowerPacket(pkt);
    else
        throw cRuntimeError("Message arrived on unknown gate: %s", arrivalGate->getFullName());
}

void NrSdap::handleUpperPacket(inet::Packet *pkt)
{
    Qfi qfi = QFI_NONE;
    bool qfiFromReflectiveQos = false;

    if (!isUe) {
        // gNB DL: the QFI always arrives from the GTP-U header, via GtpUser's QfiReq tag
        if (!pkt->hasTag<QfiReq>())
            throw cRuntimeError("SDAP TX: QfiReq tag missing on gNB DL path -- GtpUser should always set it");
        qfi = pkt->getTag<QfiReq>()->getQfi();
        EV_INFO << "SDAP TX: QFI = " << qfi << " extracted from QfiReq\n";
    }
    else {
        // UE UL: two sources may answer -- the classified QFI (a QfiReq tag, stamped by
        // the QosFlowClassifier's rules or set by the application directly) and a
        // reflective QoS match, the rule derived from observed downlink traffic. When
        // both answer, the reflectiveQosOverridesQfi switch says which wins; one alone
        // is used as-is; deliberately no per-rule precedence between the two kinds.
        Qfi taggedQfi = pkt->hasTag<QfiReq>() ? pkt->getTag<QfiReq>()->getQfi() : QFI_NONE;
        Qfi reflectiveQfi = (reflectiveQosTable != nullptr) ? reflectiveQosTable->lookupUplinkQfi(pkt) : QFI_NONE;
        if (taggedQfi != QFI_NONE && (reflectiveQfi == QFI_NONE || !reflectiveQosOverridesQfi_)) {
            qfi = taggedQfi;
            EV_INFO << "SDAP TX: QFI = " << qfi << " extracted from QfiReq\n";
        }
        else if (reflectiveQfi != QFI_NONE) {
            qfi = reflectiveQfi;
            qfiFromReflectiveQos = true;
            EV_INFO << "SDAP TX: QFI = " << qfi << " derived from reflective QoS\n";
        }
        else {
            // unclassified traffic joins the default QoS flow
            qfi = Qfi(0);
            EV_WARN << "SDAP TX: unclassified packet joins the default QoS flow (QFI 0)\n";
        }
    }

    // Lookup DRB context: nodeId is NODEID_NONE on UE, destUeId on gNB
    MacNodeId nodeId = NODEID_NONE;
    if (!isUe) {
        nodeId = pkt->getTag<FlowControlInfo>()->getDestId();
        if (nodeId == NODEID_NONE)
            EV_WARN << "SDAP TX: destId not set in FlowControlInfo\n";
    }

    const DrbDesc *drb = drbTable_.getDrbForQfi(nodeId, qfi);
    if (!drb) {
        // An uncovered QFI may still have an on-demand definition to create its DRB
        // from (see the SMF's onDemandDrbs parameter); a successful creation is
        // pushed back into this table, so retry the lookup. The SMF needs the
        // actual UE: on the gNB that is nodeId (the destination), on the UE itself
        // the flow's source.
        MacNodeId ueId = isUe ? pkt->getTag<FlowControlInfo>()->getSourceId() : nodeId;
        if (smf_->createOnDemandDrbForQfi(ueId, qfi) != DRBID_NONE)
            drb = drbTable_.getDrbForQfi(nodeId, qfi);
    }
    if (!drb) {
        drb = drbTable_.getDefaultDrb(nodeId);
        if (drb)
            EV_WARN << "SDAP TX: No DRB mapping for nodeId=" << nodeId << " QFI=" << qfi
                    << ", falling back to default DRB " << drb->getDrbId() << "\n";
    }
    if (!drb)
        throw cRuntimeError("SDAP TX: No DRB available for nodeId=%d", (int)num(nodeId));

    EV_INFO << "SDAP TX: Selected DRB=" << drb->getDrbId() << " for QFI=" << qfi << "\n";

    // Check if SDAP header is required for this DRB
    if (requiresSdapHeader(drb)) {
        // Build SDAP header according to 3GPP TS 37.324
        auto sdapHeader = makeShared<NrSdapHeader>();
        sdapHeader->setQfi(qfi);

        // Enable reflective QoS flag if this QFI supports it and we're not already using reflective QoS
        bool enableReflectiveQos = shouldEnableReflectiveQos(qfi) && !qfiFromReflectiveQos;  //TODO on gNB only?
        sdapHeader->setReflectiveQoS(enableReflectiveQos);

        pkt->insertAtFront(sdapHeader);
        EV_INFO << "SDAP TX: Inserted SDAP header with QFI = " << qfi
                << ", reflectiveQoS = " << (enableReflectiveQos ? "true" : "false") << "\n";
    }
    else {
        EV_INFO << "SDAP TX: No SDAP header required for DRB " << drb->getDrbId() << "\n";
    }

    // Set DRB ID on FlowControlInfo for PDCP/RLC entity creation and routing
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    lteInfo->setDrbId(drb->getDrbId());

    // Establish the connection unless the bearer already exists: bearers torn down at
    // handover or at a D2D mode switch are re-established by the next packet, even for
    // an already-seen (drbId, destId) pair.
    if (!establishedBearers_.count(DrbKey(lteInfo->getDestId(), drb->getDrbId()))) {
        if (!establishBearersOnDemand_)
            throw cRuntimeError("SDAP TX: no established bearer for DRB %d (peer nodeId=%d), and on-demand bearer establishment is disabled -- missing or mismatched staticBearers entry?",
                    (int)num(drb->getDrbId()), (int)num(lteInfo->getDestId()));
        // SDAP decides neither the traffic class (LCG) nor the RLC mode: RRC resolves
        // both from the bearer's configured entry at establishment (see
        // BearerManagement::resolveBearerRequest()), so the request carries no
        // configuration at all.
        smf_->establishDataConnection(lteInfo->toFlowId(), BearerRequest{CONVERSATIONAL, UNKNOWN_RLC_TYPE});
    }

    // Set protocol tag for outgoing frame to PDCP layer
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&LteProtocol::sdap);

    EV_INFO << "SDAP TX: Forwarding to DRB " << drb->getDrbId() << "\n";
    send(pkt, "pdcpOut");
}

void NrSdap::handleLowerPacket(inet::Packet *pkt)
{
    auto lteInfo = pkt->findTag<FlowControlInfo>();
    DrbId drbId = lteInfo ? lteInfo->getDrbId() : DRBID_NONE;
    MacNodeId ueId = (!isUe && lteInfo) ? lteInfo->getSourceId() : NODEID_NONE;
    const DrbDesc *drb = (drbId != DRBID_NONE) ? drbTable_.getDrb(DrbKey(ueId, drbId)) : nullptr;
    if (!drb)
        throw cRuntimeError("SDAP RX: Unknown DRB %d (ueId=%d) -- missing entry in the SMF's staticDrbs?",
                            (int)num(drbId), (int)num(ueId));

    EV_INFO << "SDAP RX: Received packet from DRB " << drbId << ": " << pkt->peekAtFront() << "\n";

    Qfi qfi = QFI_NONE;

    // Check if packet has SDAP header (should be at the front according to 3GPP TS 37.324)
    if (requiresSdapHeader(drb)) {
        // Extract SDAP header from the front of the packet
        auto sdapHeader = pkt->removeAtFront<NrSdapHeader>();
        qfi = sdapHeader->getQfi();

        EV_INFO << "SDAP RX: Extracted SDAP header with QFI = " << qfi << "\n";

        // Validate QFI range (0-63 according to 3GPP)
        if (num(qfi) > 63) {
            EV_WARN << "SDAP RX: Invalid QFI value " << qfi << " (should be 0-63)\n";
            qfi = QFI_NONE;
        }

        // Handle reflective QoS if UE and enabled
        if (sdapHeader->getReflectiveQoS()) {
            EV_INFO << "SDAP RX: Reflective QoS enabled for QFI " << qfi << "\n";
            if (isUe && reflectiveQosTable != nullptr) {
                reflectiveQosTable->handleDownlinkFlow(pkt, qfi);
            }
        }
    }
    else {
        EV_INFO << "SDAP RX: No SDAP header expected for DRB " << drbId << "\n";

        // For DRBs without SDAP header, derive QFI from DRB context (use first QFI in the list)
        if (drb && !drb->qfiList.empty()) {
            qfi = drb->qfiList[0];
            EV_INFO << "SDAP RX: Using QFI " << qfi << " from DRB context\n";
        }
    }

    // Validate QFI ↔ DRB consistency
    if (drb) {
        if (!contains(drb->qfiList, qfi))
            EV_WARN << "SDAP RX: DRB/QFI mismatch! Received on DRB=" << drbId << ", QFI=" << qfi << " not in qfiList\n";
    }

    // Add QoS indication tag for upper layers
    auto qosIndTag = pkt->addTagIfAbsent<QfiInd>();
    qosIndTag->setQfi(qfi);

    // Set protocol tag for upper layer based on PDU session type
    const inet::Protocol *upperProto = getUpperProtocol(drb);
    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(upperProto);

    EV_INFO << "SDAP RX: Forwarding packet with QFI=" << qfi << " to upper layer (protocol: " << upperProto->getName() << ")\n";
    send(pkt, "upperLayerOut");
}


} //namespace
