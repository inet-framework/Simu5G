//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "NrRlcAm.h"
//#include "buffer/NrAmTxQueue.h"
//#include "buffer/NrAmRxQueue.h"
#include "stack/rlc/am/buffer/AmRxQueue.h"
#include "stack/mac/packet/LteMacSduRequest.h"
#include "stack/rlc/am/packet/NrRlcAmStatusPdu_m.h"
//#include "buffer/NrAmRxQueue.h"
namespace simu5g {

Define_Module(NrRlcAm);

NrAmTxQueue* NrRlcAm::getNrTxBuffer(MacNodeId nodeId, LogicalCid lcid) {
    // Find TXBuffer for this CID
    MacCid cid = idToMacCid(nodeId, lcid);
    NrAmTxBuffers::iterator it = txBuffers.find(cid);

    if (it == txBuffers.end()) {
        // Not found: create
        std::stringstream buf;
        buf << "NrAmTxQueue Lcid: " << lcid << " cid: " << cid;
        cModuleType *moduleType = cModuleType::get("simu5g.stack.rlc.am.buffer.NrAmTxQueue");
        NrAmTxQueue *txbuf = check_and_cast<NrAmTxQueue *>(
                moduleType->createScheduleInit(buf.str().c_str(),
                        getParentModule()));
        txBuffers[cid] = txbuf; // Add to tx_buffers map

        EV << NOW << " NrRlcAm::getTxBuffer( Added new NrAmTxBuffer: " << txbuf->getId()
                                                      << " for node: " << nodeId << " for Lcid: " << lcid << "\n";
                                                  //  << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return txbuf;
    }
    else {
        // Found
        EV << NOW << " NrRlcAm::getTxBuffer( Using old NrAmTxBuffer: " << it->second->getId()
                                                      << " for node: " << nodeId << " for Lcid: " << lcid << "\n";
        //EV << NOW <<"; "<<this<< " NrRlcAm::getTxBuffer( Using old NrAmTxBuffer: " << it->second->getId()
        //                                        << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return check_and_cast<NrAmTxQueue *>(it->second);
    }
}
void NrRlcAm::handleUpperMessage(cPacket *pktAux)
{
    emit(receivedPacketFromUpperLayerSignal_, pktAux);
    auto pkt = check_and_cast<Packet *>(pktAux);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();

    NrAmTxQueue *txbuf = getNrTxBuffer(ctrlInfoToUeId(lteInfo), lteInfo->getLcid());

    // Create a new RLC packet
    auto rlcPkt = makeShared<LteRlcSdu>();
    rlcPkt->setSnoMainPacket(lteInfo->getSequenceNumber());
    rlcPkt->setChunkLength(B(RLC_HEADER_AM));
    rlcPkt->setLengthMainPacket(pkt->getByteLength());
    pkt->insertAtFront(rlcPkt);
    drop(pkt);
    EV << NOW << "NrRlcAm::handleUpperMessage sending to AM TX Queue sn=" << lteInfo->getSequenceNumber() <<endl;
    EV << NOW <<";"<< this<<" NrRlcAm::handleUpperMessage sending "<<pkt<<" to AM TX Queue sn=" << lteInfo->getSequenceNumber() <<endl;
    // Fragment Packet
    txbuf->enque(pkt);
}
void NrRlcAm::handleLowerMessage(cPacket *pktAux)
{
    auto pkt = check_and_cast<Packet *>(pktAux);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    auto chunk = pkt->peekAtFront<inet::Chunk>();

    if (inet::dynamicPtrCast<const LteMacSduRequest>(chunk) != nullptr) {
        // process SDU request received from MAC

        // get the corresponding Tx buffer
        NrAmTxQueue *txbuf = getNrTxBuffer(ctrlInfoToUeId(lteInfo), lteInfo->getLcid());

        auto macSduRequest = pkt->peekAtFront<LteMacSduRequest>();
        unsigned int size = macSduRequest->getSduSize();

        txbuf->sendPdus(size);

        drop(pkt);

        delete pkt;
    }
    else {
        // process AM PDU
        NrAmRxQueue *rxbuf = getNrRxBuffer(ctrlInfoToUeId(lteInfo), lteInfo->getLcid());
        drop(pkt);
        if (inet::dynamicPtrCast<const NrRlcAmStatusPdu>(chunk) != nullptr) {
            /*auto pdu = pkt->peekAtFront<LteRlcAmPdu>();
            if ((pdu->getAmType() == ACK) || (pdu->getAmType() == MRW_ACK)) {
                EV << NOW << " LteRlcAm::handleLowerMessage Received ACK message" << endl;

                // forwarding ACK to associated transmitting entity
                routeControlMessage(pkt);
                return;
            }*/
            EV<< NOW <<"; "<< this<<" NrLteRlcAm::handleLowerMessage sending control PDU "<<pkt<<" to AM TX Queue " << endl;

            rxbuf->handleControlPdu(pkt);
        } else {

            // Extract information from fragment

            emit(receivedPacketFromLowerLayerSignal_, pkt);
            EV << NOW <<";  NrLteRlcAm::handleLowerMessage sending packet to AM RX Queue " << endl;

            // Defragment packet
            rxbuf->enque(pkt);
        }
    }
}

NrAmRxQueue* NrRlcAm::getNrRxBuffer(MacNodeId nodeId, LogicalCid lcid) {
    // Find RXBuffer for this CID
    MacCid cid = idToMacCid(nodeId, lcid);

    NrAmRxBuffers::iterator it = rxBuffers.find(cid);
    if (it == rxBuffers.end()) {
        // Not found: create
        std::stringstream buf;
        buf << "NrAmRxQueue Lcid: " << lcid << " cid: " << cid;
        cModuleType *moduleType = cModuleType::get("simu5g.stack.rlc.am.buffer.NrAmRxQueue");
        NrAmRxQueue *rxbuf = check_and_cast<NrAmRxQueue *>(
                moduleType->createScheduleInit(buf.str().c_str(),
                        getParentModule()));
        rxBuffers[cid] = rxbuf; // Add to rx_buffers map
        rxbuf->setRemoteEntity(nodeId);

        EV << NOW << " NrRlcAm::getRxBuffer( Added new AmRxBuffer: " << rxbuf->getId()
                                              << " for node: " << nodeId << " for Lcid: " << lcid << "\n";
        // << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return rxbuf;
    }
    else {
        // Found
        EV << NOW << "NrRlcAm::getRxBuffer( Using old AmRxBuffer: " << it->second->getId()
                                              << " for node: " << nodeId << " for Lcid: " << lcid << "\n";
        //EV << NOW <<"; "<<this<< "NrRlcAm::getRxBuffer( Using old AmRxBuffer: " << it->second->getId()
        // << " for node: " << nodeId << " for Lcid: " << lcid << "\n";

        return check_and_cast<NrAmRxQueue*>(it->second);
    }
}
void NrRlcAm::deleteQueues(MacNodeId nodeId) {
    for (auto tit = txBuffers.begin(); tit != txBuffers.end(); ) {
        if (MacCidToNodeId(tit->first) == nodeId) {
            // cannot directly delete the module
            //delete tit->second; // Delete Queue
            tit->second->deleteModule(); // Delete Entity
            txBuffers.erase(tit++); // Delete Elem

            //delete tit->second; // Delete Queue
            // tit = txBuffers_.erase(tit); // Delete Element
        }
        else {
            ++tit;
        }
    }
    for (auto rit = rxBuffers.begin(); rit != rxBuffers.end(); ) {
        if (MacCidToNodeId(rit->first) == nodeId) {
            // cannot directly delete the module
            rit->second->deleteModule(); // Delete Entity
            //delete rit->second; // Delete Queue
            rit = rxBuffers.erase(rit); // Delete Element
        }
        else {
            ++rit;
        }
    }

}

void NrRlcAm::bufferControlPdu(cPacket *pktAux) {
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    NrAmTxQueue *txbuf = getNrTxBuffer(ctrlInfoToUeId(lteInfo), lteInfo->getLcid());
    txbuf->bufferControlPdu(pkt);
}

void NrRlcAm::routeControlMessage(cPacket *pktAux) {
    Enter_Method("routeControlMessage");

    auto pkt = check_and_cast<Packet *>(pktAux);
    auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
    NrAmTxQueue *txbuf = getNrTxBuffer(ctrlInfoToUeId(lteInfo), lteInfo->getLcid());
    // change the order of this (before handleControlPackety to avoid segmentation fault, the latter delete the pkt

    //txbuf->handleControlPacket(pkt);
    //lteInfo = pkt->removeTag<FlowControlInfo>();
    lteInfo = pkt->removeTag<FlowControlInfo>();
    txbuf->handleControlPacket(pkt);
}

} //namespace
