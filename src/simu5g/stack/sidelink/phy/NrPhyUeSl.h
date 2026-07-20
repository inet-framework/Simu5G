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

#ifndef _SIDELINK_NRPHYUESL_H_
#define _SIDELINK_NRPHYUESL_H_

#include "simu5g/stack/phy/NrPhyUe.h"
#include "simu5g/stack/sidelink/common/SlUeRadioState.h"

namespace simu5g {

/**
 * SL-aware NR Uu PHY for the UE (design decision D32, SL-3): the Uu leg of
 * the opt-in Uu/SL half-duplex arbiter. The stock Uu UE PHY records no
 * TX-interval state at all (G24); this subclass records every UL airframe
 * (data, RAC, BSR-carrying PDUs, CQI feedback) into the UE's shared
 * SlUeRadioState, and drops an arriving Uu DATA frame whose reception
 * interval overlaps an SL transmission (Uu control frames - grants, RAC
 * responses, HARQ/CQI feedback - stay lossless, consistent with the Uu
 * control model, G6).
 *
 * Swapped in unconditionally by the hasSidelink NED default of the nrPhy
 * slot; with sharedUuSlRadio=false (default) every override delegates
 * untouched - today's independent-legs behavior.
 */
class NrPhyUeSl : public NrPhyUe
{
  protected:
    bool sharedUuSlRadio_ = false;
    SlUeRadioState *radioState_ = nullptr;

    unsigned int uuHalfDuplexSlDrops_ = 0;   // Uu data frames lost to SL TX
    static omnetpp::simsignal_t uuHalfDuplexSlDropsSignal_;
    static omnetpp::simsignal_t slUuTxConflictsSignal_;

    void initialize(int stage) override;

    void handleUpperMessage(omnetpp::cMessage *msg) override;
    void handleAirFrame(omnetpp::cMessage *msg) override;
    void sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req) override;

    /// record a Uu transmission of the given duration starting now
    void recordUuTx(omnetpp::simtime_t duration);
};

} // namespace simu5g

#endif
