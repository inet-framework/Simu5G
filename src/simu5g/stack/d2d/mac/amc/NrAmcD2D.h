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

#ifndef _NRAMCD2D_H_
#define _NRAMCD2D_H_

#include "simu5g/stack/mac/amc/NrAmc.h"
#include "simu5g/stack/d2d/mac/ID2dAmc.h"
#include "simu5g/stack/d2d/mac/amc/D2dAmcHelper.h"

namespace simu5g {

/**
 * NR AMC with device-to-device (D2D) support. NR counterpart of ~LteAmcD2D:
 * it reuses the shared D2dAmcHelper for the D2D feedback / tx-param machinery
 * and only adds the thin dispatch overrides plus the NR-specific D2D MCS-table
 * routing. Keep in sync with LteAmcD2D.
 */
class NrAmcD2D : public NrAmc, public ID2dAmc
{
  protected:
    NrMcsTable d2dNrMcsTable_;

    // holds the D2D-specific AMC state and logic (shared with the LTE variant)
    D2dAmcHelper d2dHelper_;

    void initialize(int stage) override;

    // D2D branch of the base routing seams
    void printTxParamsForDirection(Direction dir, GHz carrierFrequency) override;
    bool existTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency) override;
    const UserTxParams& setTxParamsForDirection(MacNodeId id, Direction dir, UserTxParams& info, GHz carrierFrequency) override;
    const UserTxParams& getTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency) override;
    McsTable *getMcsTableForDirection(Direction dir) override;
    NrMcsTable *getNrMcsTableForDirection(Direction dir) override;
    void rescaleMcsForDirection(double rePerRb, Direction dir) override;
    void detachUserForDirection(MacNodeId nodeId, Direction dir) override;
    void attachUserForDirection(MacNodeId nodeId, Direction dir) override;
    void testUeForDirection(MacNodeId nodeId, Direction dir) override;

  public:
    NrAmcD2D() : d2dHelper_(this) {}

    // ID2dAmc
    void pushFeedbackD2D(MacNodeId id, LteFeedback fb, MacNodeId peerId, GHz carrierFrequency) override;
    const LteSummaryFeedback& getFeedbackD2D(MacNodeId id, Remote antenna, TxMode txMode, MacNodeId peerId, GHz carrierFrequency) override;
};

} //namespace

#endif
