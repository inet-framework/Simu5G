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

#ifndef _SIDELINK_SLRRC_H_
#define _SIDELINK_SLRRC_H_

#include "simu5g/stack/sidelink/common/SlBinder.h"
#include "simu5g/stack/sidelink/common/SlPreconfig.h"

namespace simu5g {

class BearerManagement;
class FlowControlInfo;

/**
 * Per-UE sidelink control plane ("genie" PC5-RRC, phase SL-1): owns the
 * sidelink preconfiguration (pool + static SLRB config), registers the UE's
 * L2 IDs / group memberships / multicast address mappings with SlBinder at
 * init, and creates SLRB entity chains via BearerManagement when SlBinder
 * fans out a connection establishment. Control plane by C++ calls only.
 */
class SlRrc : public omnetpp::cSimpleModule
{
  protected:
    SlPreconfig preconfig_;
    SlBinder *slBinder_ = nullptr;
    BearerManagement *bearerManagement_ = nullptr;

    MacNodeId nodeId_ = NODEID_NONE;   // NR node id of the owning UE
    SlL2Id srcL2Id_ = SL_L2ID_NONE;    // this UE's source Layer-2 ID

    void initialize(int stage) override;
    int numInitStages() const override;
    void handleMessage(omnetpp::cMessage *msg) override;

  public:
    const SlPreconfig& getPreconfig() const { return preconfig_; }
    MacNodeId getNodeId() const { return nodeId_; }
    SlL2Id getSrcL2Id() const { return srcL2Id_; }

    // SLRB entity chain creation (invoked via SlBinder's genie fan-out)
    void createSlOutgoingConnection(FlowControlInfo *lteInfo);
    void createSlIncomingConnection(FlowControlInfo *lteInfo);
};

} // namespace simu5g

#endif
