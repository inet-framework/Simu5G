//
//                  Simu5G
//
// Authors: Andras Varga
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "InitStages.h"

using namespace inet;

namespace simu5g {

Define_InitStage(SIMU5G_POSTLOCAL);
Define_InitStage(SIMU5G_REGISTRATIONS);
Define_InitStage(SIMU5G_REGISTRATIONS2);
Define_InitStage(SIMU5G_NODE_RELATIONSHIPS);
Define_InitStage(SIMU5G_BINDER_ACCESS);
Define_InitStage_Dependency(SIMU5G_POSTLOCAL, LOCAL);
Define_InitStage_Dependency(SIMU5G_REGISTRATIONS, SIMU5G_POSTLOCAL);
Define_InitStage_Dependency(SIMU5G_REGISTRATIONS2, SIMU5G_REGISTRATIONS);
Define_InitStage_Dependency(SIMU5G_NODE_RELATIONSHIPS, SIMU5G_REGISTRATIONS2);
Define_InitStage_Dependency(SIMU5G_BINDER_ACCESS, SIMU5G_NODE_RELATIONSHIPS);
Define_InitStage_Dependency(SINGLE_MOBILITY, SIMU5G_BINDER_ACCESS);
Define_InitStage_Dependency(GROUP_MOBILITY, SIMU5G_BINDER_ACCESS);

Define_InitStage(SIMU5G_AMC_SETUP)
Define_InitStage_Dependency(SIMU5G_AMC_SETUP, SIMU5G_BINDER_ACCESS);

Define_InitStage(SIMU5G_PHYSICAL_LAYER)
Define_InitStage_Dependency(SIMU5G_PHYSICAL_LAYER, SIMU5G_AMC_SETUP);

Define_InitStage(SIMU5G_MAC_SCHEDULER_CREATION)
Define_InitStage_Dependency(SIMU5G_MAC_SCHEDULER_CREATION, SIMU5G_PHYSICAL_LAYER);

Define_InitStage(SIMU5G_BACKGROUNDTRAFFICMANAGER)
Define_InitStage_Dependency(SIMU5G_BACKGROUNDTRAFFICMANAGER, SIMU5G_MAC_SCHEDULER_CREATION);

Define_InitStage(SIMU5G_TTI_SETUP)
Define_InitStage_Dependency(SIMU5G_TTI_SETUP, SIMU5G_BACKGROUNDTRAFFICMANAGER);

Define_InitStage(SIMU5G_CELLINFO_CHANNELUPDATE)
Define_InitStage_Dependency(SIMU5G_CELLINFO_CHANNELUPDATE, SIMU5G_TTI_SETUP);

Define_InitStage(SIMU5G_AMC_ATTACHUSER)
Define_InitStage_Dependency(SIMU5G_AMC_ATTACHUSER, SIMU5G_CELLINFO_CHANNELUPDATE);
Define_InitStage_Dependency(SIMU5G_SERVICE_PLATFORM_SETUP, SIMU5G_AMC_ATTACHUSER);

// NOTE: The following dependencies are SOLELY for demonstrating that above Simu5G
// stages DO NOT DEPEND on inet's stages. These dependencies can be removed at any time.

Define_InitStage_Dependency(PHYSICAL_ENVIRONMENT, SIMU5G_AMC_ATTACHUSER);
Define_InitStage_Dependency(POWER, SIMU5G_AMC_ATTACHUSER);
Define_InitStage_Dependency(CLOCK, SIMU5G_AMC_ATTACHUSER);
Define_InitStage_Dependency(NETWORK_INTERFACE_CONFIGURATION, SIMU5G_AMC_ATTACHUSER);

Define_InitStage(SIMU5G_SERVICE_PLATFORM_SETUP)
Define_InitStage_Dependency(SIMU5G_SERVICE_PLATFORM_SETUP, TRANSPORT_LAYER);
Define_InitStage_Dependency(APPLICATION_LAYER, SIMU5G_SERVICE_PLATFORM_SETUP);

} // namespace simu5g
