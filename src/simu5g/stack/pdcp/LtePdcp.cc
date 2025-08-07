//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/pdcp/LtePdcp.h"

#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include <inet/transportlayer/tcp_common/TcpHeader.h>
#include <inet/transportlayer/udp/UdpHeader_m.h>
#include <inet/common/packet/chunk/SequenceChunk.h>
#include <inet/common/ProtocolTag_m.h>

#include "simu5g/stack/packetFlowManager/PacketFlowManagerBase.h"
#include "simu5g/stack/pdcp/packet/RohcHeader.h"
#include "simu5g/stack/sdap/packet/NrSdapHeader_m.h"
#include "simu5g/common/LteControlInfoTags_m.h"

namespace simu5g {

Define_Module(LtePdcpUe);
Define_Module(LtePdcpEnb);

using namespace omnetpp;
using namespace inet;

// We require a minimum length of 1 Byte for each header even in compressed state
// (transport, network and ROHC header, i.e. minimum is 3 Bytes)
#define MIN_COMPRESSED_HEADER_SIZE    B(3)

// statistics
simsignal_t LtePdcpBase::receivedPacketFromUpperLayerSignal_ = registerSignal("receivedPacketFromUpperLayer");
simsignal_t LtePdcpBase::receivedPacketFromLowerLayerSignal_ = registerSignal("receivedPacketFromLowerLayer");
simsignal_t LtePdcpBase::sentPacketToUpperLayerSignal_ = registerSignal("sentPacketToUpperLayer");
simsignal_t LtePdcpBase::sentPacketToLowerLayerSignal_ = registerSignal("sentPacketToLowerLayer");

LtePdcpBase::~LtePdcpBase()
{
}

bool LtePdcpBase::isCompressionEnabled()
{
    return headerCompressedSize_ != LTE_PDCP_HEADER_COMPRESSION_DISABLED;
}

void LtePdcpBase::headerCompress(Packet *pkt)
{
    if (isCompressionEnabled()) {
        // Check PacketProtocolTag to determine packet type
        auto protocolTag = pkt->findTag<PacketProtocolTag>();
        inet::Ptr<inet::Chunk> sdapHeader = nullptr;

        // If packet has SDAP protocol tag, remove SDAP header first
        if (protocolTag && protocolTag->getProtocol() == &LteProtocol::sdap) {
            sdapHeader = pkt->removeAtFront<NrSdapHeader>();
            EV << "LtePdcp : Removed SDAP header before compression\n";
        }

        // Extract IP and transport headers to be compressed
        auto ipHeader = pkt->removeAtFront<Ipv4Header>();
        inet::Ptr<inet::Chunk> transportHeader = nullptr;

        int transportProtocol = ipHeader->getProtocolId();

        if (IP_PROT_TCP == transportProtocol) {
            transportHeader = pkt->removeAtFront<tcp::TcpHeader>();
        }
        else if (IP_PROT_UDP == transportProtocol) {
            transportHeader = pkt->removeAtFront<UdpHeader>();
        }

        // Create a sequence chunk containing the original headers
        auto originalHeaders = inet::makeShared<inet::SequenceChunk>();

        // Make headers immutable before inserting into SequenceChunk
        ipHeader->markImmutable();
        originalHeaders->insertAtBack(ipHeader);
        if (transportHeader) {
            transportHeader->markImmutable();
            originalHeaders->insertAtBack(transportHeader);
        }

        // Make originalHeaders immutable before passing to RohcHeader
        originalHeaders->markImmutable();

        // Create ROHC header with original headers and compressed size
        auto rohcHeader = makeShared<RohcHeader>(originalHeaders, headerCompressedSize_);
        rohcHeader->markImmutable();
        pkt->insertAtFront(rohcHeader);

        // If we had an SDAP header, add it back on top of the ROHC header
        if (sdapHeader) {
            sdapHeader->markImmutable();
            pkt->insertAtFront(sdapHeader);
            EV << "LtePdcp : Added SDAP header back on top of ROHC header\n";
        }

        EV << "LtePdcp : Header compression performed\n";
    }
}

void LtePdcpBase::headerDecompress(Packet *pkt)
{
    if (isCompressionEnabled()) {
        pkt->trim();

        // Check if there's an SDAP header on top
        inet::Ptr<inet::Chunk> sdapHeader = nullptr;
        if (pkt->peekAtFront<NrSdapHeader>()) {
            sdapHeader = pkt->removeAtFront<NrSdapHeader>();
            EV << "LtePdcp : Removed SDAP header before decompression\n";
        }

        auto rohcHeader = pkt->removeAtFront<RohcHeader>();

        // Get the original headers from the ROHC header
        auto originalHeaders = rohcHeader->getChunk();

        // Insert the original headers back into the packet
        pkt->insertAtFront(originalHeaders);

        // If we had an SDAP header, add it back on top
        if (sdapHeader) {
            pkt->insertAtFront(sdapHeader);
            EV << "LtePdcp : Added SDAP header back on top after decompression\n";
        }

        EV << "LtePdcp : Header decompression performed\n";
    }
}

void LtePdcpBase::setTrafficInformation(cPacket *pkt, inet::Ptr<FlowControlInfo> lteInfo)
{
    if ((strcmp(pkt->getName(), "VoIP")) == 0) {
        lteInfo->setTraffic(CONVERSATIONAL);
        lteInfo->setRlcType(conversationalRlc_);
    }
    else if ((strcmp(pkt->getName(), "gaming")) == 0) {
        lteInfo->setTraffic(INTERACTIVE);
        lteInfo->setRlcType(interactiveRlc_);
    }
    else if ((strcmp(pkt->getName(), "VoDPacket") == 0)
             || (strcmp(pkt->getName(), "VoDFinishPacket") == 0))
    {
        lteInfo->setTraffic(STREAMING);
        lteInfo->setRlcType(streamingRlc_);
    }
    else {
        lteInfo->setTraffic(BACKGROUND);
        lteInfo->setRlcType(backgroundRlc_);
    }

    lteInfo->setDirection(getDirection());
}

/*
 * Upper Layer handlers
 */

void LtePdcpBase::fromDataPort(cPacket *pktAux)
{
    emit(receivedPacketFromUpperLayerSignal_, pktAux);

    // Control Information
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto lteInfo = pkt->addTagIfAbsent<FlowControlInfo>();

    setTrafficInformation(pkt, lteInfo);

    // Get IP flow information from the new tag
    auto ipFlowInd = pkt->getTag<IpFlowInd>();
    Ipv4Address srcAddr = ipFlowInd->getSrcAddr();
    Ipv4Address dstAddr = ipFlowInd->getDstAddr();
    uint16_t typeOfService = ipFlowInd->getTypeOfService();

    bool useNR = pkt->getTag<TechnologyReq>()->getUseNR();
    MacNodeId destId = getDestId(dstAddr, useNR, lteInfo->getSourceId());

    // CID Request
    EV << "LteRrc : Received CID request for Traffic [ " << "Source: " << srcAddr
       << " Destination: " << dstAddr
       << " ToS: " << typeOfService << " ]\n";

    // TODO: Since IP addresses can change when we add and remove nodes, maybe node IDs should be used instead of them
    LogicalCid mylcid;
    if ((mylcid = ht_.find_entry(srcAddr.getInt(), dstAddr.getInt(), typeOfService)) == 0xFFFF) {
        // LCID not found
        mylcid = lcid_++;

        EV << "LteRrc : Connection not found, new CID created with LCID " << mylcid << "\n";

        ht_.create_entry(srcAddr.getInt(), dstAddr.getInt(), typeOfService, mylcid);
    }

    // assign LCID and node IDs
    lteInfo->setLcid(mylcid);
    lteInfo->setSourceId(nodeId_);
    lteInfo->setDestId(destId);

    // obtain CID
    MacCid cid = idToMacCid(destId, mylcid);

    EV << "LteRrc : Assigned Lcid: " << mylcid << "\n";
    EV << "LteRrc : Assigned Node ID: " << nodeId_ << "\n";

    // get the PDCP entity for this LCID and process the packet
    LteTxPdcpEntity *entity = getTxEntity(cid);
    entity->handlePacketFromUpperLayer(pkt);
}

/*
 * Lower layer handlers
 */

void LtePdcpBase::fromLowerLayer(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    emit(receivedPacketFromLowerLayerSignal_, pkt);

    auto lteInfo = pkt->getTag<FlowControlInfo>();

    MacCid cid = idToMacCid(lteInfo->getSourceId(), lteInfo->getLcid());   // TODO: check if you have to get master node id

    LteRxPdcpEntity *entity = getRxEntity(cid);
    entity->handlePacketFromLowerLayer(pkt);
}

void LtePdcpBase::toDataPort(cPacket *pktAux)
{
    Enter_Method_Silent("LtePdcpBase::toDataPort");

    auto pkt = check_and_cast<Packet *>(pktAux);
    take(pkt);

    EV << "LtePdcp : Sending packet " << pkt->getName() << " on port DataPort$o\n";

    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ipv4);

    EV << "LtePdcp : Sending packet " << pkt->getName() << " on port DataPort$o\n";

    // Send message
    send(pkt, dataPortOutGate_);
    emit(sentPacketToUpperLayerSignal_, pkt);
}

/*
 * Forwarding Handlers
 */

void LtePdcpBase::sendToLowerLayer(Packet *pkt)
{
    auto lteInfo = pkt->getTag<FlowControlInfo>();

    cGate *gate;
    switch (lteInfo->getRlcType()) {
        case UM:
            gate = umSapOutGate_;
            break;
        case AM:
            gate = amSapOutGate_;
            break;
        case TM:
            gate = tmSapOutGate_;
            break;
        default:
            throw cRuntimeError("LtePdcpBase::sendToLowerLayer(): invalid RlcType %d", lteInfo->getRlcType());
    }

    EV << "LtePdcp : Sending packet " << pkt->getName() << " on port " << gate->getFullName() << endl;

    /*
     * @author Alessandro Noferi
     *
     * Since the other methods, e.g. fromData, are overridden
     * in many classes, this method is the only one used by
     * all the classes (except the NRPdcpUe that has its
     * own sendToLowerLayer method).
     * So, the notification about the new PDCP to the pfm
     * is done here.
     *
     * packets sent in D2D mode are not considered
     */

    if (lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D) {
        if (packetFlowManager_ != nullptr)
            packetFlowManager_->insertPdcpSdu(pkt);
    }

    // Send message
    send(pkt, gate);
    emit(sentPacketToLowerLayerSignal_, pkt);
}

void LtePdcpBase::sendToUpperLayer(cPacket *pkt)
{
    EV << "LtePdcp : Sending packet " << pkt->getName() << " on port DataPort$o" << endl;

    // Send message
    send(pkt, dataPortOutGate_);
    emit(sentPacketToUpperLayerSignal_, pkt);
}

/*
 * Main functions
 */

void LtePdcpBase::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        dataPortInGate_ = gate("DataPort$i");
        dataPortOutGate_ = gate("DataPort$o");
        tmSapInGate_ = gate("TM_Sap$i", 0);
        tmSapOutGate_ = gate("TM_Sap$o", 0);
        umSapInGate_ = gate("UM_Sap$i", 0);
        umSapOutGate_ = gate("UM_Sap$o", 0);
        amSapInGate_ = gate("AM_Sap$i", 0);
        amSapOutGate_ = gate("AM_Sap$o", 0);

        binder_.reference(this, "binderModule", true);
        headerCompressedSize_ = B(par("headerCompressedSize"));
        if (headerCompressedSize_ != LTE_PDCP_HEADER_COMPRESSION_DISABLED &&
            headerCompressedSize_ < MIN_COMPRESSED_HEADER_SIZE) {
            throw cRuntimeError("Size of compressed header must not be less than %" PRId64 "B.", MIN_COMPRESSED_HEADER_SIZE.get());
        }

        nodeId_ = MacNodeId(getContainingNode(this)->par("macNodeId").intValue());

        packetFlowManager_.reference(this, "packetFlowManagerModule", false);
        NRpacketFlowManager_.reference(this, "nrPacketFlowManagerModule", false);

        if (packetFlowManager_) {
            EV << "LtePdcpBase::initialize - PacketFlowManager present" << endl;
        }
        if (NRpacketFlowManager_) {
            EV << "LtePdcpBase::initialize - NRpacketFlowManager present" << endl;
        }

        conversationalRlc_ = aToRlcType(par("conversationalRlc"));
        interactiveRlc_ = aToRlcType(par("interactiveRlc"));
        streamingRlc_ = aToRlcType(par("streamingRlc"));
        backgroundRlc_ = aToRlcType(par("backgroundRlc"));

        // TODO WATCH_MAP(gatemap_);
        WATCH(headerCompressedSize_);
        WATCH(nodeId_);
        WATCH(lcid_);
    }
}

void LtePdcpBase::handleMessage(cMessage *msg)
{
    cPacket *pkt = check_and_cast<cPacket *>(msg);
    EV << "LtePdcp : Received packet " << pkt->getName() << " from port "
       << pkt->getArrivalGate()->getName() << endl;

    cGate *incoming = pkt->getArrivalGate();
    if (incoming == dataPortInGate_) {
        fromDataPort(pkt);
    }
    else {
        fromLowerLayer(pkt);
    }
}

LteTxPdcpEntity *LtePdcpBase::getTxEntity(MacCid cid)
{
    // Find entity for this LCID
    PdcpTxEntities::iterator it = txEntities_.find(cid);
    if (it == txEntities_.end()) {
        // Not found: create
        std::stringstream buf;
        // FIXME HERE

        buf << "LteTxPdcpEntity cid: " << cid;
        cModuleType *moduleType = cModuleType::get("simu5g.stack.pdcp.LteTxPdcpEntity");
        LteTxPdcpEntity *txEnt = check_and_cast<LteTxPdcpEntity *>(moduleType->createScheduleInit(buf.str().c_str(), this));
        txEntities_[cid] = txEnt;    // Add to entities map

        EV << "LtePdcpBase::getTxEntity - Added new TxPdcpEntity for Cid: " << cid << "\n";

        return txEnt;
    }
    else {
        // Found
        EV << "LtePdcpBase::getTxEntity - Using old TxPdcpEntity for Cid: " << cid << "\n";

        return it->second;
    }
}

LteRxPdcpEntity *LtePdcpBase::getRxEntity(MacCid cid)
{
    // Find entity for this CID
    PdcpRxEntities::iterator it = rxEntities_.find(cid);
    if (it == rxEntities_.end()) {
        // Not found: create

        std::stringstream buf;
        buf << "LteRxPdcpEntity Cid: " << cid;
        cModuleType *moduleType = cModuleType::get("simu5g.stack.pdcp.LteRxPdcpEntity");
        LteRxPdcpEntity *rxEnt = check_and_cast<LteRxPdcpEntity *>(moduleType->createScheduleInit(buf.str().c_str(), this));
        rxEntities_[cid] = rxEnt;    // Add to entities map

        EV << "LtePdcpBase::getRxEntity - Added new RxPdcpEntity for Cid: " << cid << "\n";

        return rxEnt;
    }
    else {
        // Found
        EV << "LtePdcpBase::getRxEntity - Using old RxPdcpEntity for Cid: " << cid << "\n";

        return it->second;
    }
}

void LtePdcpBase::finish()
{
    // TODO make-finish
}

void LtePdcpEnb::initialize(int stage)
{
    LtePdcpBase::initialize(stage);
}

void LtePdcpEnb::deleteEntities(MacNodeId nodeId)
{
    Enter_Method_Silent();

    // delete connections related to the given UE
    for (auto tit = txEntities_.begin(); tit != txEntities_.end(); ) {
        if (MacCidToNodeId(tit->first) == nodeId) {
            tit->second->deleteModule();
            tit = txEntities_.erase(tit);
        }
        else {
            ++tit;
        }
    }

    for (auto rit = rxEntities_.begin(); rit != rxEntities_.end(); ) {
        if (MacCidToNodeId(rit->first) == nodeId) {
            rit->second->deleteModule();
            rit = rxEntities_.erase(rit);
        }
        else {
            ++rit;
        }
    }
}

void LtePdcpUe::deleteEntities(MacNodeId nodeId)
{
    // delete all connections TODO: check this (for NR dual connectivity)
    for (auto& [txId, txEntity] : txEntities_) {
        txEntity->deleteModule();  // Delete Entity
    }
    txEntities_.clear(); // Clear all entities after deletion

    for (auto& [rxId, rxEntity] : rxEntities_) {
        rxEntity->deleteModule();  // Delete Entity
    }
    rxEntities_.clear(); // Clear all entities after deletion
}

void LtePdcpUe::initialize(int stage)
{
    LtePdcpBase::initialize(stage);
    if (stage == inet::INITSTAGE_NETWORK_LAYER) {
        // refresh value, the parameter may have changed between INITSTAGE_LOCAL and INITSTAGE_NETWORK_LAYER
        nodeId_ = MacNodeId(getContainingNode(this)->par("macNodeId").intValue());
    }
}

} //namespace
