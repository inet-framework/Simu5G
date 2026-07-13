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

#ifndef _LTE_ID2DAMC_H_
#define _LTE_ID2DAMC_H_

#include "simu5g/common/LteCommon.h"
#include "simu5g/stack/phy/feedback/LteFeedback.h"
#include "simu5g/stack/phy/feedback/LteSummaryFeedback.h"

namespace simu5g {

/*
 * Interface implemented by every D2D-capable AMC (LteAmcD2D / NrAmcD2D).
 *
 * D2D code talks to the serving cell's AMC D2D feedback machinery through this
 * interface instead of a concrete class, so that LTE (LteAmcD2D) and NR
 * (NrAmcD2D) D2D AMCs need no common D2D base class. Obtain it with
 * check_and_cast<ID2dAmc *>(amc): the AMC of a D2D-capable node is always a
 * D2D AMC (the D2D NIC wires amcType to LteAmcD2D/NrAmcD2D).
 */
class ID2dAmc
{
  public:
    virtual ~ID2dAmc() {}

    /// Store a D2D feedback sample reported by transmitter id towards peer peerId.
    virtual void pushFeedbackD2D(MacNodeId id, LteFeedback fb, MacNodeId peerId, GHz carrierFrequency) = 0;

    /// Retrieve the D2D feedback summary from transmitter id towards peer peerId.
    virtual const LteSummaryFeedback& getFeedbackD2D(MacNodeId id, Remote antenna, TxMode txMode, MacNodeId peerId, GHz carrierFrequency) = 0;
};

} //namespace

#endif
