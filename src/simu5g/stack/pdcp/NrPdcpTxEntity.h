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

#ifndef _NRPDCPTXENTITY_H_
#define _NRPDCPTXENTITY_H_

#include "simu5g/stack/pdcp/LtePdcpTxEntity.h"

namespace simu5g {

/**
 * @class NrPdcpTxEntity
 * @brief NR flavor of the transmitting PDCP entity.
 *
 * Adds the NR per-SDU statistics (pdcpSduSentNr) and the NR-leg source id of a
 * single-leg NR bearer at a UE. On a multi-leg bearer the enclosing compound's
 * splitter handles leg dispatch, id mapping and per-leg statistics instead
 * (see PdcpEntityBase.ned), and this entity just forwards.
 */
class NrPdcpTxEntity : public LtePdcpTxEntity
{
    static simsignal_t pdcpSduSentNrSignal_;

  protected:
    // NR node ID (of NR-capable UEs)
    MacNodeId nrNodeId_ = NODEID_NONE;

    // deliver the PDCP PDU to the lower layer
    void deliverPdcpPdu(Packet *pkt) override;

  public:
    void initialize(int stage) override;
};

} //namespace

#endif
