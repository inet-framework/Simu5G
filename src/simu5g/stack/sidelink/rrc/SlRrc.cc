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

#include "simu5g/stack/sidelink/rrc/SlRrc.h"

#include <set>

#include <inet/common/InitStages.h>
#include <inet/common/ModuleAccess.h>
#include <omnetpp/cvaluemap.h>

#include "simu5g/stack/rrc/BearerManagement.h"

namespace simu5g {

using namespace omnetpp;

Define_Module(SlRrc);

int SlRrc::numInitStages() const
{
    return inet::NUM_INIT_STAGES;
}

void SlRrc::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        cModule *node = inet::getContainingNode(this);
        nodeId_ = MacNodeId(node->par("nrMacNodeId").intValue());

        // parse the preconfiguration (pool + slrbConfig)
        auto *cfg = check_and_cast<cValueMap *>(par("preconfig").objectValue());
        preconfig_.loadFromJson(cfg);

        // D3 invariant: DRB ids must be unique per destination L2 ID (2-field
        // DrbKey stays collision-free because TX keys use the destination L2Pid)
        std::set<int> drbs;
        for (const auto& e : preconfig_.slrbConfig)
            if (!drbs.insert(num(e.drbId)).second)
                throw cRuntimeError("SlRrc: duplicate DRB id %hu in slrbConfig (DRB ids must be unique, see D3)", num(e.drbId));

        // own source L2 ID: explicit parameter, or derived from the node id
        long l2IdPar = par("srcL2Id").intValue();
        srcL2Id_ = (l2IdPar >= 0) ? (SlL2Id)l2IdPar : (SlL2Id)num(nodeId_);

        bearerManagement_ = check_and_cast<BearerManagement *>(getModuleByPath(par("bearerManagementModule").stringValue()));

        // registrations with the global SL registry
        slBinder_ = SlBinder::getInstance();
        slBinder_->registerUeL2Id(srcL2Id_, nodeId_);
        slBinder_->registerSlRrc(nodeId_, this);
        for (const auto& e : preconfig_.slrbConfig) {
            if (e.castType == SL_BROADCAST || e.castType == SL_GROUPCAST) {
                slBinder_->getOrAssignGroupL2Pid(e.dstL2Id);
                // SL-1: every SL UE with this SLRB configured listens to the destination
                slBinder_->joinGroup(e.dstL2Id, nodeId_);
            }
            if (!e.destAddress.empty())
                slBinder_->registerMulticastAddress(inet::Ipv4Address(e.destAddress.c_str()), e.dstL2Id);
        }
        slBinder_->registerSlCarrier(GHz(preconfig_.carrierFrequencyGHz), preconfig_.numerologyIndex,
                preconfig_.subchannelSize, preconfig_.numSubchannels);

        EV << "SlRrc::initialize - node " << nodeId_ << ", srcL2Id " << srcL2Id_
           << ", carrier " << preconfig_.carrierFrequencyGHz << " GHz (mu=" << preconfig_.numerologyIndex
           << "), " << preconfig_.slrbConfig.size() << " SLRB(s) configured" << endl;
    }
}

void SlRrc::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module does not process messages");
}

void SlRrc::createSlOutgoingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createSlOutgoingConnection()");
    bearerManagement_->createSlOutgoingConnection(lteInfo);
}

void SlRrc::createSlIncomingConnection(FlowControlInfo *lteInfo)
{
    Enter_Method_Silent("createSlIncomingConnection()");
    bearerManagement_->createSlIncomingConnection(lteInfo);
}

} // namespace simu5g
