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

#include "simu5g/stack/d2d/mac/amc/LteAmcD2D.h"
#include "simu5g/common/InitStages.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(LteAmcD2D);

void LteAmcD2D::initialize(int stage)
{
    LteAmc::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS)
        d2dHelper_.initD2D();
}

void LteAmcD2D::printTxParamsForDirection(Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        d2dHelper_.printTxParamsD2D(carrierFrequency);
    else
        LteAmc::printTxParamsForDirection(dir, carrierFrequency);
}

bool LteAmcD2D::existTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.existTxParamsD2D(id, carrierFrequency);
    return LteAmc::existTxParamsForDirection(id, dir, carrierFrequency);
}

const UserTxParams& LteAmcD2D::setTxParamsForDirection(MacNodeId id, Direction dir, UserTxParams& info, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.setTxParamsD2D(id, info, carrierFrequency);
    return LteAmc::setTxParamsForDirection(id, dir, info, carrierFrequency);
}

const UserTxParams& LteAmcD2D::getTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.getTxParamsD2D(id, carrierFrequency);
    return LteAmc::getTxParamsForDirection(id, dir, carrierFrequency);
}

McsTable *LteAmcD2D::getMcsTableForDirection(Direction dir)
{
    if (dir == D2D || dir == D2D_MULTI)
        return &ulMcsTable_;
    return LteAmc::getMcsTableForDirection(dir);
}

void LteAmcD2D::rescaleMcsForDirection(double rePerRb, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.rescaleD2D(rePerRb);
    else
        LteAmc::rescaleMcsForDirection(rePerRb, dir);
}

void LteAmcD2D::detachUserForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.detachUserD2D(nodeId);
    else
        LteAmc::detachUserForDirection(nodeId, dir);
}

void LteAmcD2D::attachUserForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.attachUserD2D(nodeId);
    else
        LteAmc::attachUserForDirection(nodeId, dir);
}

void LteAmcD2D::testUeForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.testUeD2D(nodeId);
    else
        LteAmc::testUeForDirection(nodeId, dir);
}

void LteAmcD2D::pushFeedbackD2D(MacNodeId id, LteFeedback fb, MacNodeId peerId, GHz carrierFrequency)
{
    d2dHelper_.pushFeedbackD2D(id, fb, peerId, carrierFrequency);
}

const LteSummaryFeedback& LteAmcD2D::getFeedbackD2D(MacNodeId id, Remote antenna, TxMode txMode, MacNodeId peerId, GHz carrierFrequency)
{
    return d2dHelper_.getFeedbackD2D(id, antenna, txMode, peerId, carrierFrequency);
}

} //namespace
