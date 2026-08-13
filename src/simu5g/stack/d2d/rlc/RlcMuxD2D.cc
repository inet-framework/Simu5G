//
//                  Simu5G
//
// Copyright (C) 2012-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/d2d/rlc/RlcMuxD2D.h"
#include "simu5g/stack/d2d/rlc/ID2dRlcUmTxEntity.h"
#include "simu5g/stack/rlc/RlcTxEntityBase.h"
#include "simu5g/stack/rlc/RlcRxEntityBase.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/d2d/rrc/D2DModeSwitchNotification_m.h"

namespace simu5g {

Define_Module(RlcMuxD2D);

// This module emits no D2D-specific signals of its own; the ones it forwards are
// owned by RlcMux. See the "Signals" note in D2dUeMacBase.h for the package rule.

void RlcMuxD2D::fromMacLayer(cPacket *pktAux)
{
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto chunk = pkt->peekAtFront<inet::Chunk>();

    // D2D: handle mode switch notification
    if (inet::dynamicPtrCast<const D2DModeSwitchNotification>(chunk) != nullptr) {
        EV << "RlcMuxD2D::fromMacLayer - Received packet " << pkt->getName() << " from lower layer\n";
        auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
        auto switchPkt = pkt->peekAtFront<D2DModeSwitchNotification>();

        // The notification chunk carries its own BearerRequest, filled at the eNB build
        // sites from the same connection descriptor that populates the tag below.
        FlowId flow = lteInfo->toFlowId();
        BearerRequest req{(LteTrafficClass)switchPkt->getLcg(), (LteRlcType)switchPkt->getRlcType()};

        if (switchPkt->getTxSide()) {
            // get the corresponding Tx buffer & call handler
            DrbKey id = flow.txDrbKey();
            RlcTxEntityBase *txbuf = bearerManagement_->lookupRlcTxBuffer(id);
            if (txbuf == nullptr)
                txbuf = bearerManagement_->createRlcTxBuffer(id, flow, req);
            auto *umTxbuf = dynamic_cast<ID2dRlcUmTxEntity *>(txbuf);
            if (umTxbuf == nullptr)
                throw cRuntimeError("RlcMuxD2D::fromMacLayer: D2D mode switch received for a non-UM TX bearer (%s): D2D mode switching supports UM bearers only", txbuf->getClassName());
            umTxbuf->rlcHandleD2DModeSwitch(switchPkt->getOldConnection(), switchPkt->getClearRlcBuffer());

            delete pkt;
        }
        else { // rx side
            DrbKey id = flow.rxDrbKey();
            // The mux keeps only a DRB -> toRxEntity gate-index table, so reach the
            // entity through the gate it serves; create the bearer if it has none yet.
            auto it = rxGateIndices_.find(id);
            RlcRxEntityBase *rxbuf = it != rxGateIndices_.end()
                    ? check_and_cast<RlcRxEntityBase *>(gate("toRxEntity", it->second)->getPathEndGate()->getOwnerModule())
                    : bearerManagement_->createRlcRxBuffer(id, flow, req);
            auto *umRxbuf = dynamic_cast<ID2dRlcUmRxEntity *>(rxbuf);
            if (umRxbuf == nullptr)
                throw cRuntimeError("RlcMuxD2D::fromMacLayer: D2D mode switch received for a non-UM RX bearer (%s): D2D mode switching supports UM bearers only", rxbuf->getClassName());
            umRxbuf->rlcHandleD2DModeSwitch(switchPkt->getOldConnection(), switchPkt->getOldMode(), switchPkt->getClearRlcBuffer());

            delete pkt;
        }
        return;
    }

    RlcMux::fromMacLayer(pktAux);
}

} // namespace simu5g
