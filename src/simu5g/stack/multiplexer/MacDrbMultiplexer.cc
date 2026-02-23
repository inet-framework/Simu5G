//
//                  Simu5G
//
// Authors: Mohamed Seliem (University College Cork)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "MacDrbMultiplexer.h"
#include "simu5g/common/RadioBearerTag_m.h"

namespace simu5g {

Define_Module(MacDrbMultiplexer);

void MacDrbMultiplexer::initialize()
{
    // Optional: resolve SDAP for fallback lookup (gNB UL-first scenario)
    sdap_ = dynamic_cast<NrSdap *>(getModuleByPath("^.sdap"));

    WATCH_MAP(macCidToDrb_);
}

void MacDrbMultiplexer::handleMessage(cMessage *msg)
{
    auto pkt = check_and_cast<inet::Packet *>(msg);
    cGate *incomingGate = pkt->getArrivalGate();

    if (incomingGate == gate("macIn")) {
        // MAC → RLC: look up the correct RLC gate index from the learned mapping
        auto lteInfo = pkt->getTag<FlowControlInfo>();
        // On gNB, sourceId is the UE; on UE, sourceId is the gNB (use destId for DL)
        MacNodeId peerId = (lteInfo->getDirection() == DL) ? lteInfo->getDestId() : lteInfo->getSourceId();
        MacCid cid(peerId, lteInfo->getLcid());
        auto it = macCidToDrb_.find(cid);
        int drbIndex;
        if (it != macCidToDrb_.end()) {
            drbIndex = it->second;
        }
        else if (sdap_) {
            // Fallback: ask SDAP for (ueNodeId, lcid) -> drbIndex
            drbIndex = sdap_->getDrbIndexForMacCid(peerId, lteInfo->getLcid());
            if (drbIndex >= 0) {
                macCidToDrb_[cid] = drbIndex;  // cache for next time
                EV_INFO << "MacDrbMultiplexer: resolved CID=" << cid << " -> drbIndex=" << drbIndex << " via SDAP\n";
            }
            else {
                drbIndex = 0;
                EV_WARN << "MacDrbMultiplexer: SDAP has no mapping for CID=" << cid << ", using drbIndex=0\n";
            }
        }
        else {
            // No SDAP: use lcid directly (works when lcid == local DRB index)
            int numDrbs = gateSize("rlcOut");
            drbIndex = std::min((int)lteInfo->getLcid(), numDrbs - 1);
            EV_WARN << "MacDrbMultiplexer: no mapping for CID=" << cid << ", fallback drbIndex=" << drbIndex << "\n";
        }
        send(pkt, "rlcOut", drbIndex);
    }
    else {
        // RLC → MAC: learn the MacCid-to-gate mapping from the incoming gate index
        auto lteInfo = pkt->findTag<FlowControlInfo>();
        if (lteInfo != nullptr) {
            MacNodeId peerId = (lteInfo->getDirection() == DL) ? lteInfo->getDestId() : lteInfo->getSourceId();
            MacCid cid(peerId, lteInfo->getLcid());
            int gateIndex = incomingGate->getIndex();
            macCidToDrb_[cid] = gateIndex;
        }
        send(pkt, "macOut");
    }
}

} //namespace
