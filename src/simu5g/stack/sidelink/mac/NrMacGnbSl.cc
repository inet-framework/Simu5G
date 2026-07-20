//
//                  Simu5G
//
// Copyright (C) 2026 OpenSim Ltd.
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/sidelink/mac/NrMacGnbSl.h"

#include "simu5g/stack/mac/packet/LteMacPdu.h"
#include "simu5g/stack/sidelink/mac/SlSchedulingGrant_m.h"
#include "simu5g/stack/sidelink/rrc/SlGnbRrc.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

Define_Module(NrMacGnbSl);

void NrMacGnbSl::initialize(int stage)
{
    NrMacGnb::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL)
        slGnbRrc_ = check_and_cast<SlGnbRrc *>(getModuleByPath(par("slGnbRrcModule").stringValue()));
}

void NrMacGnbSl::macPduUnmake(cPacket *cpkt)
{
    auto pkt = check_and_cast<Packet *>(cpkt);
    auto userInfo = pkt->getTag<UserControlInfo>();

    if (userInfo->getPacketLcid() == SL_SHORT_BSR) {
        // an SL-BSR-only PDU (D26): consumed entirely here - it must never
        // reach bufferizeBsr()/backlog(), which would hand the CID to the Uu
        // UL scheduler and allocate Uu RBs for sidelink backlog (G21)
        auto macPdu = pkt->removeAtFront<LteMacPdu>();
        MacNodeId ueId = userInfo->getSourceId();
        ASSERT(!macPdu->hasSdu());  // BSR-only by construction (NrMacUeSl)

        while (macPdu->hasCe()) {
            MacBsr *bsr = check_and_cast<MacBsr *>(macPdu->popCe());
            int reportedBytes = bsr->getSize();
            delete bsr;  // G23: this override consumes the CE, so it deletes it

            EV << NOW << " NrMacGnbSl::macPduUnmake - SL-BSR from UE " << ueId
               << " (" << reportedBytes << "B)" << endl;

            SlEnbScheduler::GrantSpec spec = slGnbRrc_->onSlBsr(ueId, reportedBytes);
            if (spec.isValid())
                sendSlGrant(ueId, spec, userInfo->getCarrierFrequency());
        }

        pkt->insertAtFront(macPdu);
        delete pkt;
        return;
    }

    NrMacGnb::macPduUnmake(cpkt);
}

void NrMacGnbSl::sendSlGrant(MacNodeId ueId, const SlEnbScheduler::GrantSpec& spec, GHz carrierFreq)
{
    // the sendGrants packet-construction pattern (seam 2): the DCI rides the
    // Uu DL control path (GRANTPKT, lossless per the Uu control model)
    auto pkt = new Packet("SlGrant");
    auto grant = makeShared<SlSchedulingGrant>();
    grant->setDirection(SL);
    grant->setChunkLength(b(1));  // the NrMacGnb grant-size convention (G23)
    grant->setSlFirstSlot(spec.firstSlot);
    grant->setNumOccasions(spec.numOccasions);
    grant->setOccasionGapSlots(spec.occasionGapSlots);
    grant->setSlFirstSubchannel(spec.firstSubchannel);
    grant->setSlNumSubchannels(spec.numSubchannels);
    grant->setSlMcs(spec.mcs);
    pkt->insertAtFront(grant);

    auto uinfo = pkt->addTagIfAbsent<UserControlInfo>();
    uinfo->setSourceId(getMacNodeId());
    uinfo->setDestId(ueId);
    uinfo->setFrameType(GRANTPKT);
    uinfo->setCarrierFrequency(carrierFreq);

    EV << NOW << " NrMacGnbSl::sendSlGrant - SL grant to UE " << ueId << " on carrier "
       << carrierFreq << endl;

    sendLowerPackets(pkt);
}

} // namespace simu5g
