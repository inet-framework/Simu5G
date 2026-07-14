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

#ifndef _LTE_AIRPHYENBD2D_H_
#define _LTE_AIRPHYENBD2D_H_

#include "simu5g/stack/phy/LtePhyEnb.h"

namespace simu5g {

using namespace omnetpp;

class D2dBinder;

class LtePhyEnbD2D : public LtePhyEnb
{

    bool enableD2DCqiReporting_;

    // holder of the global D2D state, resolved (find-or-create) at init
    D2dBinder *d2dBinder_ = nullptr;

  protected:

    void initialize(int stage) override;
    void requestFeedback(UserControlInfo *lteinfo, LteAirFrame *frame, inet::Packet *pkt) override;
    void handleAirFrame(cMessage *msg) override;

    /**
     * The eNB/gNB is the sole sender of D2D mode-switch notifications. Such
     * frames get a distinctive name and an elevated (-1) scheduling priority so
     * they are processed ahead of ordinary air-frames; every other frame follows
     * the generic ~LtePhyBase transmit path.
     */
    void handleUpperMessage(cMessage *msg) override;
};

} //namespace

#endif /* _LTE_AIRPHYENBD2D_H_ */
