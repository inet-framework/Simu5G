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

#ifndef _LTE_AIRPHYUED2D_H_
#define _LTE_AIRPHYUED2D_H_

#include "simu5g/stack/phy/LtePhyUe.h"
#include "simu5g/stack/d2d/phy/D2dUePhyHelper.h"

namespace simu5g {

using namespace omnetpp;

class LtePhyUeD2D : public LtePhyUe
{
  protected:

    // holds the D2D-specific UE-PHY state and logic (shared with the NR variant):
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

#endif /* _LTE_AIRPHYUED2D_H_ */

