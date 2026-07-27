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

#include "simu5g/stack/pdcp/NrTxPdcpEntity.h"

namespace simu5g {

Define_Module(NrTxPdcpEntity);

simsignal_t NrTxPdcpEntity::pdcpSduSentNrSignal_ = registerSignal("pdcpSduSentNr");

void NrTxPdcpEntity::initialize(int stage)
{
    LteTxPdcpEntity::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        if (getNodeTypeById(nodeId_) == UE)
            nrNodeId_ = MacNodeId(getContainingNode(this)->par("nrMacNodeId").intValue());
    }
}

void NrTxPdcpEntity::deliverPdcpPdu(Packet *pkt)
{
    if (!emitPerSduSignals_) {
        // multi-leg bearer: the compound's splitter does leg dispatch, id mapping and statistics
        send(pkt, "out");
        return;
    }

    if (getNodeTypeById(nodeId_) == UE) {
        // single-leg NR bearer of an NR UE: NR-leg source id + NR-flavored statistics
        auto lteInfo = pkt->getTagForUpdate<FlowControlInfo>();
        EV << NOW << " NrTxPdcpEntity::deliverPdcpPdu - DRB ID[" << lteInfo->getDrbId() << "] - sending packet to NR RLC" << endl;
        lteInfo->setSourceId(nrNodeId_);
        if (hasListeners(pdcpSduSentNrSignal_) && lteInfo->getDirection() != D2D_MULTI && lteInfo->getDirection() != D2D) {
            emit(pdcpSduSentNrSignal_, pkt);
        }
        emit(sentPacketToLowerLayerSignal_, pkt);
        send(pkt, "out");
    }
    else { // gNB: same as the base entity
        LteTxPdcpEntity::deliverPdcpPdu(pkt);
    }
}

} //namespace
