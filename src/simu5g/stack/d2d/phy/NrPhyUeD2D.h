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

#ifndef _NRPHYUED2D_H_
#define _NRPHYUED2D_H_

#include "simu5g/stack/phy/NrPhyUe.h"
#include "simu5g/stack/d2d/phy/D2dUePhyHelper.h"

namespace simu5g {

using namespace omnetpp;

/**
 * NR-UE PHY with D2D support.
 *
 * Mirrors ~LtePhyUeD2D on top of the NR base ~NrPhyUe: the D2D-specific state
 * and logic (D2D Tx power and the D2D-multicast capture-effect machinery) live
 * in the shared D2dUePhyHelper, while handleAirFrame() is the NR version that
 * previously lived directly in NrPhyUe.
 */
class NrPhyUeD2D : public NrPhyUe
{
  protected:

    // holds the D2D-specific UE-PHY state and logic (shared with the LTE variant):
    // D2D Tx power and the D2D-multicast capture-effect machinery
    D2dUePhyHelper d2dHelper_{this};

    // timer for triggering decoding at the end of the TTI. Started when the first
    // airframe is received. The self-message stays in the leaf (the module owns it);
    // the captured frames it decodes live in d2dHelper_.
    cMessage *d2dDecodingTimer_ = nullptr;

    void initialize(int stage) override;
    void handleAirFrame(cMessage *msg) override;
    void handleUpperMessage(cMessage *msg) override;
    void handleSelfMessage(cMessage *msg) override;

  public:

    void sendFeedback(LteFeedbackDoubleVector fbDl, LteFeedbackDoubleVector fbUl, FeedbackRequest req) override;
    double getTxPwr(Direction dir = UNKNOWN_DIRECTION) override
    {
        if (dir == D2D)
            return d2dHelper_.getD2dTxPower();
        return txPower_;
    }

};

} //namespace

#endif /* _NRPHYUED2D_H_ */
