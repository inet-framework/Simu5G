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

#include "simu5g/stack/pdcp/LteRxPdcpEntity.h"
#include "simu5g/common/LteCommon.h"
#include "simu5g/common/LteControlInfo.h"
#include <inet/networklayer/ipv4/Ipv4Header_m.h>
#include <inet/transportlayer/tcp_common/TcpHeader.h>
#include <inet/transportlayer/udp/UdpHeader_m.h>
#include "simu5g/stack/pdcp/packet/LteRohcPdu_m.h"
#include "simu5g/stack/pdcp/packet/LtePdcpPdu_m.h"

namespace simu5g {

Define_Module(LteRxPdcpEntity);


void LteRxPdcpEntity::initialize()
{
    pdcp_ = check_and_cast<LtePdcpBase *>(getParentModule());

    headerCompressionEnabled_ = pdcp_->par("headerCompressedSize").intValue() > 0;  // TODO a.k.a. LTE_PDCP_HEADER_COMPRESSION_DISABLED
}

void LteRxPdcpEntity::handlePacketFromLowerLayer(Packet *pkt)
{
    EV << NOW << " LteRxPdcpEntity::handlePacketFromLowerLayer - LCID[" << lcid_ << "] - processing packet from RLC layer" << endl;

    // Extract sequence number from PDCP header before popping it
    auto pdcpHeader = pkt->peekAtFront<LtePdcpHeader>();
    unsigned int sequenceNumber = pdcpHeader->getSequenceNumber();

    // PacketFlowManager functionality removed

    // pop PDCP header
    pkt->popAtFront<LtePdcpHeader>();

    // perform PDCP operations
    decompressHeader(pkt); // Decompress packet header

    // handle PDCP SDU
    handlePdcpSdu(pkt, sequenceNumber);
}

void LteRxPdcpEntity::handlePdcpSdu(Packet *pkt, unsigned int sequenceNumber)
{
    Enter_Method("LteRxPdcpEntity::handlePdcpSdu");

    EV << NOW << " LteRxPdcpEntity::handlePdcpSdu - processing PDCP SDU with SN[" << sequenceNumber << "]" << endl;

    // deliver to IP layer
    pdcp_->toDataPort(pkt);
}

void LteRxPdcpEntity::decompressHeader(Packet *pkt)
{
    if (isCompressionEnabled()) {
        pkt->trim();
        auto rohcHeader = pkt->removeAtFront<LteRohcPdu>();
        auto ipHeader = pkt->removeAtFront<Ipv4Header>();
        int transportProtocol = ipHeader->getProtocolId();

        if (IP_PROT_TCP == transportProtocol) {
            auto tcpHeader = pkt->removeAtFront<tcp::TcpHeader>();
            tcpHeader->setChunkLength(rohcHeader->getOrigSizeTransportHeader());
            pkt->insertAtFront(tcpHeader);
        }
        else if (IP_PROT_UDP == transportProtocol) {
            auto udpHeader = pkt->removeAtFront<UdpHeader>();
            udpHeader->setChunkLength(rohcHeader->getOrigSizeTransportHeader());
            pkt->insertAtFront(udpHeader);
        }
        else {
            EV_WARN << "LtePdcp : unknown transport header - cannot perform transport header decompression";
        }

        ipHeader->setChunkLength(rohcHeader->getOrigSizeIpHeader());
        pkt->insertAtFront(ipHeader);

        EV << "LtePdcp : Header decompression performed\n";
    }
}


} //namespace
