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

#include "simu5g/stack/d2d/mac/amc/NrAmcD2D.h"
#include "simu5g/common/InitStages.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(NrAmcD2D);

void NrAmcD2D::initialize(int stage)
{
    NrAmc::initialize(stage);
    if (stage == INITSTAGE_SIMU5G_BINDER_ACCESS)
        d2dHelper_.initD2D();
}

void NrAmcD2D::printTxParamsForDirection(Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        d2dHelper_.printTxParamsD2D(carrierFrequency);
    else
        NrAmc::printTxParamsForDirection(dir, carrierFrequency);
}

bool NrAmcD2D::existTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.existTxParamsD2D(id, carrierFrequency);
    return NrAmc::existTxParamsForDirection(id, dir, carrierFrequency);
}

const UserTxParams& NrAmcD2D::setTxParamsForDirection(MacNodeId id, Direction dir, UserTxParams& info, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.setTxParamsD2D(id, info, carrierFrequency);
    return NrAmc::setTxParamsForDirection(id, dir, info, carrierFrequency);
}

const UserTxParams& NrAmcD2D::getTxParamsForDirection(MacNodeId id, Direction dir, GHz carrierFrequency)
{
    if (dir == D2D)
        return d2dHelper_.getTxParamsD2D(id, carrierFrequency);
    return NrAmc::getTxParamsForDirection(id, dir, carrierFrequency);
}

McsTable *NrAmcD2D::getMcsTableForDirection(Direction dir)
{
    if (dir == D2D || dir == D2D_MULTI)
        return &ulMcsTable_;
    return NrAmc::getMcsTableForDirection(dir);
}

NrMcsTable *NrAmcD2D::getNrMcsTableForDirection(Direction dir)
{
    if (dir == D2D || dir == D2D_MULTI)
        return &ulNrMcsTable_;
    return NrAmc::getNrMcsTableForDirection(dir);
}

void NrAmcD2D::rescaleMcsForDirection(double rePerRb, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.rescaleD2D(rePerRb);
    else
        NrAmc::rescaleMcsForDirection(rePerRb, dir);
}

void NrAmcD2D::detachUserForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.detachUserD2D(nodeId);
    else
        NrAmc::detachUserForDirection(nodeId, dir);
}

void NrAmcD2D::attachUserForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.attachUserD2D(nodeId);
    else
        NrAmc::attachUserForDirection(nodeId, dir);
}

void NrAmcD2D::testUeForDirection(MacNodeId nodeId, Direction dir)
{
    if (dir == D2D)
        d2dHelper_.testUeD2D(nodeId);
    else
        NrAmc::testUeForDirection(nodeId, dir);
}

void NrAmcD2D::pushFeedbackD2D(MacNodeId id, LteFeedback fb, MacNodeId peerId, GHz carrierFrequency)
{
    d2dHelper_.pushFeedbackD2D(id, fb, peerId, carrierFrequency);
}

const LteSummaryFeedback& NrAmcD2D::getFeedbackD2D(MacNodeId id, Remote antenna, TxMode txMode, MacNodeId peerId, GHz carrierFrequency)
{
    return d2dHelper_.getFeedbackD2D(id, antenna, txMode, peerId, carrierFrequency);
}

} //namespace
