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

#ifndef _LTE_LTEAMCD2D_H_
#define _LTE_LTEAMCD2D_H_

#include "simu5g/stack/mac/amc/LteAmc.h"
#include "simu5g/stack/d2d/mac/ID2dAmc.h"
#include "simu5g/stack/d2d/mac/amc/D2dAmcHelper.h"

namespace simu5g {

/**
 * LTE AMC with device-to-device (D2D) support: adds the D2D feedback and
 * transmission-parameter machinery on top of the clean LteAmc. The D2D state
 * and heavy logic live in the shared D2dAmcHelper; this leaf only overrides the
 * base routing seams and the ID2dAmc surface to delegate to it. It is the LTE
 * counterpart of ~NrAmcD2D; keep the two in sync.
 */
class LteAmcD2D : public LteAmc, public ID2dAmc
{
  protected:
    // holds the D2D-specific AMC state and logic (shared with the NR variant)
    D2dAmcHelper d2dHelper_;

    void initialize(int stage) override;

    // D2D branch of the base routing seams
    void printTxParamsForDirection(Direction dir, GHz carrierFrequency) override;
    bool existTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency) override;
    const UserTxParams& setTxParamsForDirection(MacNodeId id, Direction dir, UserTxParams& info, GHz carrierFrequency) override;
    const UserTxParams& getTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency) override;
    McsTable *getMcsTableForDirection(Direction dir) override;
    void rescaleMcsForDirection(double rePerRb, Direction dir) override;
    void detachUserForDirection(MacNodeId nodeId, Direction dir) override;
    void attachUserForDirection(MacNodeId nodeId, Direction dir) override;
    void testUeForDirection(MacNodeId nodeId, Direction dir) override;

  public:
    LteAmcD2D() : d2dHelper_(this) {}

    // ID2dAmc
    void pushFeedbackD2D(MacNodeId id, LteFeedback fb, MacNodeId peerId, GHz carrierFrequency) override;
    const LteSummaryFeedback& getFeedbackD2D(MacNodeId id, Remote antenna, TxMode txMode, MacNodeId peerId, GHz carrierFrequency) override;
};

} //namespace

#endif
