//
//                  Simu5G
//
// Copyright (C) 2012 Antonio Virdis, Daniele Migliorini, Matteo Maria Andreozzi,
//   Giovanni Accongiagioco, Generoso Pagano, Vincenzo Pii (SimuLTE)
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <inet/networklayer/common/NetworkInterface.h>
#include <inet/common/ProtocolTag_m.h>

#include "simu5g/stack/rlc/um/RlcUmTxEntityBase.h"
#include "simu5g/stack/mac/LteMacBase.h"
#include "simu5g/stack/mac/packet/LteMacSduRequest.h"
#include "simu5g/stack/rlc/packet/LteRlcNewDataTag_m.h"
#include "simu5g/stack/rlc/packet/PdcpTrackingTag_m.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"
#include "simu5g/stack/rrc/D2DModeController.h"

namespace simu5g {

using namespace inet;

// Registered here (single translation unit) so the signal-id assignment order is
// independent of the LTE/NR concrete .cc files.
simsignal_t RlcUmTxEntityBase::rlcPduCreatedSignal_ = registerSignal("rlcPduCreated");
simsignal_t RlcUmTxEntityBase::wastedGrantedBytesSignal_ = registerSignal("wastedGrantedBytes");
simsignal_t RlcUmTxEntityBase::requestedPduSizeSignal_ = registerSignal("requestedPduSize");
simsignal_t RlcUmTxEntityBase::sentPduSizeSignal_ = registerSignal("sentPduSize");
simsignal_t RlcUmTxEntityBase::receivedPacketFromUpperLayerSignal_ = registerSignal("receivedPacketFromUpperLayer");
simsignal_t RlcUmTxEntityBase::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");

void RlcUmTxEntityBase::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        LteMacBase *mac = getModuleFromPar<LteMacBase>(par("macModule"), this);
        ownerNodeId_ = mac->getMacNodeId();

        initMode();

        // D2D mode-switch controller is optional (absent on non-D2D stacks) -> findModuleFromPar.
        d2dModeController_ = findModuleFromPar<D2DModeController>(par("d2dModeControllerModule"), this);
    }
}

void RlcUmTxEntityBase::setFlowControlInfo(FlowControlInfo *info)
{
    RlcTxEntityBase::setFlowControlInfo(info);
    // Record this flow's wire format on the (dup'd) flow info; it rides on every
    // packet tagged for the MAC, so the scheduler/MAC can multiplex multiple SO
    // PDUs into one grant (NR keeps one SDU/segment per PDU, no RLC concatenation).
    if (flowControlInfo_) {
        flowControlInfo_->setSoFraming(usesSoFraming());
        flowControlInfo_->setRlcSnFieldLength(snFieldLength());
    }
}

void RlcUmTxEntityBase::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *incoming = pkt->getArrivalGate();
    if (incoming->isName("in")) {
        handleSdu(pkt);
    }
    else if (incoming->isName("macIn")) {
        handleMacSduRequest(pkt);
    }
    else {
        throw cRuntimeError("RlcUmTxEntity: unexpected message from gate %s", incoming->getFullName());
    }
}

void RlcUmTxEntityBase::handleSdu(inet::Packet *pkt)
{
    EV << NOW << " RlcUmTxEntity::handleSdu - Received SDU from upper layer, size " << pkt->getByteLength() << "\n";

    // Add PDCP tracking information (SN + original length), read back in rlcPduMake().
    auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
    auto pdcpTag = pkt->addTag<PdcpTrackingTag>();
    pdcpTag->setPdcpSequenceNumber(pdcpHeader->getSequenceNumber());
    pdcpTag->setOriginalPacketLength(pkt->getByteLength());

    if (holdingDownstreamInPackets_) {
        // D2D mode switch in progress on the new-mode entity: hold the SDU and
        // do not notify the MAC until the old-mode entity has drained.
        EV << "RlcUmTxEntity::handleSdu - Enqueue packet into the Holding Buffer\n";
        enqueHoldingPackets(pkt);
        return;
    }

    bool stored = storeSdu(pkt);

    if (stored) {
        // Notify the MAC that the queue has new data. The dup carries the full SDU
        // length, so the MAC tracks remaining bytes itself across grant requests.
        auto pktDup = pkt->dup();
        pktDup->addTag<LteRlcNewDataTag>();
        // This indication creates/updates the MAC outgoing connection; stamp the
        // flow's wire format on it so the scheduler multiplexes multiple SO PDUs
        // per grant (NR keeps one SDU/segment per PDU, no RLC concatenation).
        pktDup->getTagForUpdate<FlowControlInfo>()->setSoFraming(usesSoFraming());
        pktDup->getTagForUpdate<FlowControlInfo>()->setRlcSnFieldLength(snFieldLength());
        EV << "RlcUmTxEntity::handleSdu - Sending new data indication to MAC\n";
        send(pktDup, "out");
    }
    else {
        // Queue is full - drop SDU (LTE mode only; NR buffer is unbounded)
        dropBufferOverflow(pkt);
    }
}

void RlcUmTxEntityBase::handleMacSduRequest(inet::Packet *pkt)
{
    // Enter_Method + take needed for context switch when called via gate from the RlcMux
    Enter_Method("handleMacSduRequest()");
    if (pkt->getOwner() != this)
        take(pkt);

    auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
    unsigned int size = macSduRequest->getSduSize();

    EV << NOW << " RlcUmTxEntity::handleMacSduRequest - MAC requests PDU of size " << size << "\n";

    // do segmentation/concatenation and send a PDU to the lower layer
    rlcPduMake(size);

    delete pkt;
}

void RlcUmTxEntityBase::enqueHoldingPackets(cPacket *pkt)
{
    EV << NOW << " RlcUmTxEntity::enqueHoldingPackets - storing new SDU into the holding buffer " << endl;
    sduHoldingQueue_.insert(pkt);
}

void RlcUmTxEntityBase::dropBufferOverflow(cPacket *pkt)
{
    EV << "RlcUmTxEntity : Dropping packet " << pkt->getName() << " (queue full) \n";
    delete pkt;
}

void RlcUmTxEntityBase::sendPduToMac(inet::Packet *pkt)
{
    pkt->addTagIfAbsent<inet::PacketProtocolTag>()->setProtocol(&LteProtocol::rlc);
    send(pkt, "out");
}

} //namespace
