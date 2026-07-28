//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
// Editor: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/d2d/mac/LteMacEnbD2D.h"

#include "simu5g/stack/mac/packet/LteSchedulingGrant.h"
#include "simu5g/stack/mac/scheduler/LteSchedulerEnbUl.h"
#include "simu5g/stack/packetFlowObserver/PacketFlowSignals.h"

namespace simu5g {

Define_Module(LteMacEnbD2D);

using namespace omnetpp;
using namespace inet;

void LteMacEnbD2D::macPduUnmake(cPacket *cpkt)
{
    auto pkt = check_and_cast<Packet *>(cpkt);
    auto macPdu = pkt->removeAtFront<LteMacPdu>();
    auto userInfo = pkt->getTag<UserControlInfo>();

    // Notify the packet flow manager about the successful arrival of a TB from a UE.
    // From ETSI TS 138314 V16.0.0 (2020-07)
    if (hasListeners(ulMacPduArrivedSignal_)) {
        GrantSignalInfo ulInfo(userInfo->getSourceId(), userInfo->getGrantId());
        emit(ulMacPduArrivedSignal_, &ulInfo);
    }

    while (macPdu->hasSdu()) {
        // Extract and send SDU
        LogicalCid lcid;
        auto upPkt = macPdu->popSdu(lcid);
        take(upPkt);

        EV << "LteMacEnbD2D: pduUnmaker extracted SDU" << endl;

        MacNodeId senderId = userInfo->getSourceId();
        MacCid cid = MacCid(senderId, lcid);
        ASSERT(connDescIn_.find(cid) != connDescIn_.end());
        *upPkt->addTag<FlowControlInfo>() = connDescIn_[cid].toFlowControlInfo();

        EV << "LteMacEnbD2D: Lcid --->"<< (int)lcid << " Cid: " << cid <<endl;

        sendUpperPackets(upPkt);
    }

    while (macPdu->hasCe()) {
        // Extract CE
        // TODO: see if for cid or lcid
        MacBsr *bsr = check_and_cast<MacBsr *>(macPdu->popCe());
        auto lteInfo = pkt->getTag<UserControlInfo>();
        LogicalCid lcid = lteInfo->getPacketLcid();  // one of SHORT_BSR or D2D_MULTI_SHORT_BSR

        MacCid cid = MacCid(lteInfo->getSourceId(), lcid); // this way, different connections from the same UE (e.g. one UL and one D2D)
                                                               // obtain different CIDs. With the inverse operation, you can get
                                                               // the LCID and discover if the connection is UL or D2D
        bufferizeBsr(bsr, cid);
    }
    pkt->insertAtFront(macPdu);

    delete pkt;
}

void LteMacEnbD2D::sendGrants(std::map<GHz, LteMacScheduleList> *scheduleList)
{
    EV << NOW << "LteMacEnbD2D::sendGrants " << endl;

    for (auto& [carrierFreq, carrierScheduleList] : *scheduleList) {
        while (!carrierScheduleList.empty()) {
            LteMacScheduleList::iterator it, ot;
            it = carrierScheduleList.begin();

            Codeword cw = it->first.second;
            Codeword otherCw = MAX_CODEWORDS - cw;
            MacCid cid = it->first.first;
            LogicalCid lcid = cid.getLcid();
            MacNodeId nodeId = cid.getNodeId();
            unsigned int granted = it->second;
            unsigned int codewords = 0;

            // removing visited element from scheduleList.
            carrierScheduleList.erase(it);

            if (granted > 0) {
                // increment number of allocated Cw
                ++codewords;
            }
            else {
                // active cw becomes the "other one"
                cw = otherCw;
            }

            std::pair<MacCid, Codeword> otherPair(MacCid(nodeId, LogicalCid(0)), otherCw);

            if ((ot = (carrierScheduleList.find(otherPair))) != (carrierScheduleList.end())) {
                // increment number of allocated Cw
                ++codewords;

                // removing visited element from scheduleList.
                carrierScheduleList.erase(ot);
            }

            if (granted == 0)
                continue; // avoiding transmission of 0 grant (0 grant should not be created)

            EV << NOW << " LteMacEnbD2D::sendGrants Node[" << getMacNodeId() << "] - "
               << granted << " blocks to grant for user " << nodeId << " on "
               << codewords << " codewords. CW[" << cw << "\\" << otherCw << "] carrier[" << carrierFreq << "]" << endl;

            // get the direction of the grant, depending on which connection has been scheduled by the eNB
            Direction dir = directionFromBsrLcid(lcid, UL);

            // TODO Grant is set aperiodic as default
            // TODO: change to tag instead of header
            auto pkt = new Packet("LteGrant");
            auto grant = makeShared<LteSchedulingGrant>();
            grant->setDirection(dir);
            grant->setCodewords(codewords);

            // set total granted blocks
            grant->setTotalGrantedBlocks(granted);
            grant->setChunkLength(b(1));

            pkt->addTagIfAbsent<UserControlInfo>()->setSourceId(getMacNodeId());
            pkt->addTagIfAbsent<UserControlInfo>()->setDestId(nodeId);
            pkt->addTagIfAbsent<UserControlInfo>()->setFrameType(GRANTPKT);
            pkt->addTagIfAbsent<UserControlInfo>()->setCarrierFrequency(carrierFreq);

            const UserTxParams& ui = getAmc()->computeTxParams(nodeId, dir, carrierFreq);
            UserTxParams *txPara = new UserTxParams(ui);
            // FIXME: possible memory leak
            grant->setUserTxParams(txPara);

            // acquiring remote antennas set from user info
            const std::set<Remote>& antennas = ui.readAntennaSet();

            // get bands for this carrier
            const unsigned int firstBand = cellInfo_->getCarrierStartingBand(carrierFreq);
            const unsigned int lastBand = cellInfo_->getCarrierLastBand(carrierFreq);

            //  HANDLE MULTICW
            for ( ; cw < codewords; ++cw) {
                unsigned int grantedBytes = 0;

                for (Band b = firstBand; b <= lastBand; ++b) {
                    unsigned int bandAllocatedBlocks = 0;
                    for (const auto& antenna : antennas) {
                        bandAllocatedBlocks += enbSchedulerUl_->readPerUeAllocatedBlocks(nodeId, antenna, b);
                    }
                    grantedBytes += amc_->computeBytesOnNRbs(nodeId, b, cw, bandAllocatedBlocks, dir, carrierFreq);
                }

                grant->setGrantedCwBytes(cw, grantedBytes);
                EV << NOW << " LteMacEnbD2D::sendGrants - granting " << grantedBytes << " on cw " << cw << endl;
            }
            RbMap map;

            enbSchedulerUl_->readRbOccupation(nodeId, carrierFreq, map);

            grant->setGrantedBlocks(map);

            /*
             * @author Alessandro Noferi
             * Notify the packet flow manager about the successful arrival of a TB from a UE.
             * From ETSI TS 138314 V16.0.0 (2020-07)
             *   tSched: the point in time when the UL MAC SDU i is scheduled as
             *   per the scheduling grant provided
             */
            if (hasListeners(grantSentSignal_)) {
                GrantSignalInfo grantInfo(nodeId, grant->getGrantId());
                emit(grantSentSignal_, &grantInfo);
            }

            // send grant to PHY layer
            pkt->insertAtFront(grant);
            sendLowerPackets(pkt);
        }
    }
}

} //namespace
