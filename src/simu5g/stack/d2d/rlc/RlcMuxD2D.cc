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
#include "simu5g/stack/d2d/rlc/UmTxEntityD2D.h"
#include "simu5g/stack/d2d/rlc/UmRxEntityD2D.h"
#include "simu5g/stack/rrc/BearerManagement.h"
#include "simu5g/stack/d2d/rrc/D2DModeSwitchNotification_m.h"

namespace simu5g {

Define_Module(RlcMuxD2D);

// NOTE: no static registerSignal() calls in this translation unit -- all
// signals used by this module are registered in RlcMux.cc (global signal-ID
// order stability, sz fingerprint).

void RlcMuxD2D::fromMacLayer(cPacket *pktAux)
{
    auto pkt = check_and_cast<inet::Packet *>(pktAux);
    auto chunk = pkt->peekAtFront<inet::Chunk>();

    // D2D: handle mode switch notification
    if (inet::dynamicPtrCast<const D2DModeSwitchNotification>(chunk) != nullptr) {
        EV << "RlcMuxD2D::fromMacLayer - Received packet " << pkt->getName() << " from lower layer\n";
        auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
        auto switchPkt = pkt->peekAtFront<D2DModeSwitchNotification>();

        if (switchPkt->getTxSide()) {
            // get the corresponding Tx buffer & call handler
            DrbKey id = ctrlInfoToTxDrbKey(lteInfo.get());
            RlcTxEntityBase *txbuf = bearerManagement_->lookupRlcTxBuffer(id);
            if (txbuf == nullptr)
                txbuf = bearerManagement_->createRlcTxBuffer(id, lteInfo.get());
            UmTxEntityD2D *umTxbuf = dynamic_cast<UmTxEntityD2D *>(txbuf);
            if (umTxbuf == nullptr)
                throw cRuntimeError("RlcMuxD2D::fromMacLayer: D2D mode switch received for a non-UM TX bearer (%s): D2D mode switching supports UM bearers only", txbuf->getClassName());
            umTxbuf->rlcHandleD2DModeSwitch(switchPkt->getOldConnection(), switchPkt->getClearRlcBuffer());

            delete pkt;
        }
        else { // rx side
            DrbKey id = ctrlInfoToRxDrbKey(lteInfo.get());
            RlcRxEntityBase *rxbuf = lookupRxBuffer(id);
            if (rxbuf == nullptr)
                rxbuf = bearerManagement_->createRlcRxBuffer(id, lteInfo.get());
            UmRxEntityD2D *umRxbuf = dynamic_cast<UmRxEntityD2D *>(rxbuf);
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
