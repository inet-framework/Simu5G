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
 * @brief NR flavor of the transmitting PDCP entity.
 *
 * At a UE, signals each SDU of a single-leg bearer by the technology of the
 * bearer's leg: pdcpSduSentNr on the NR leg, pdcpSduSentLte on the LTE leg. On a
 * multi-leg bearer the enclosing compound's splitter handles leg dispatch, id
 * mapping and per-leg statistics instead (see PdcpEntityBase.ned), and this
 * entity just forwards.
 */
class NrPdcpTxEntity : public LtePdcpTxEntity
{
    static simsignal_t pdcpSduSentNrSignal_;

  protected:
    // deliver the PDCP PDU to the lower layer
    void deliverPdcpPdu(Packet *pkt) override;
};

} //namespace

#endif
