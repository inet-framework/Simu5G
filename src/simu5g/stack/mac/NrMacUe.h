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

#ifndef _NRMACUE_H_
#define _NRMACUE_H_

#include "simu5g/stack/mac/LteMacUe.h"

namespace simu5g {

class NrMacUe : public LteMacUe
{
  public:
    NrMacUe();

  protected:
    /**
     * Main loop
     */
    void handleSelfMessage() override;

    /// NR carriers are served on their own numerology period
    bool isCarrierActive(GHz carrierFrequency) override
    {
        return getNumerologyPeriodCounter(binder_->getNumerologyIndexFromCarrierFreq(carrierFrequency)) == 0;
    }

    /// asynchronous H-ARQ: pick an empty unit within the first available process
    UnitList reserveTxHarqUnits(LteHarqBufferTx *txBuf, Direction dir) override;

    /**
     * macPduMake() creates MAC PDUs (one for each CID)
     * by extracting SDUs from Real Mac Buffers according
     * to the Schedule List.
     * It sends them to H-ARQ (at the moment lower layer)
     *
     * On UE it also adds a BSR control element to the MAC PDU
     * containing the size of its buffer (for that CID)
     */
    /// NR bearers are peer-addressed: the destination comes from the flow, not the cell.
    MacNodeId pduDestId(MacCid destCid) override { return connDescOut_.at(destCid).flowInfo.getDestId(); }

    /// An entry with no SDUs is only worth a PDU if it can carry a pending BSR.
    bool shouldSkipScheduleEntry(unsigned int sduPerCid) const override { return sduPerCid == 0 && !isBsrPending(); }

    inet::Packet *createUlMacPdu(MacCid destCid, GHz carrierFreq, MacNodeId destId) override;
};

} //namespace

#endif
