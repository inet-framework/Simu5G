//
//                  Simu5G
//
// Copyright (C) 2019-2021 Giovanni Nardini, Giovanni Stea, Antonio Virdis et al. (University of Pisa)
// Copyright (C) 2022-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "simu5g/stack/pdcp/NrPdcpTxEntity.h"

namespace simu5g {

Define_Module(NrPdcpTxEntity);

simsignal_t NrPdcpTxEntity::pdcpSduSentNrSignal_ = registerSignal("pdcpSduSentNr");

void NrPdcpTxEntity::deliverPdcpPdu(Packet *pkt)
{
    if (!emitPerSduSignals_) {
        // multi-leg bearer: the compound's splitter does leg dispatch, id mapping and statistics
        send(pkt, "out");
        return;
    }

    if (getNodeTypeById(nodeId_) == UE) {
        // single-leg bearer of a UE: the flow's source id is already that of the bearer's
        // leg (the anchor stack's under dual connectivity, else the attached stack's), and
        // the per-SDU signal is the one of the leg's technology
        auto lteInfo = pkt->getTag<FlowControlInfo>();
        bool isNrLeg = isNrUe(lteInfo->getSourceId());
        EV << NOW << " NrPdcpTxEntity::deliverPdcpPdu - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to the " << (isNrLeg ? "NR" : "LTE") << " RLC" << endl;
        simsignal_t signal = isNrLeg ? pdcpSduSentNrSignal_ : pdcpSduSentLteSignal_;
        if (hasListeners(signal) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D) {
            emit(signal, pkt);
        }
        emit(sentPacketToLowerLayerSignal_, pkt);
        send(pkt, "out");
    }
    else { // gNB: same as the base entity
        LtePdcpTxEntity::deliverPdcpPdu(pkt);
    }
}

} //namespace
