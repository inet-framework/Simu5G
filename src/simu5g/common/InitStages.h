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

#ifndef __SIMU5G_INITSTAGES_H
#define __SIMU5G_INITSTAGES_H

#include <inet/common/InitStages.h>

namespace simu5g {

extern const inet::InitStage INITSTAGE_SIMU5G_POSTLOCAL;
extern const inet::InitStage INITSTAGE_SIMU5G_REGISTRATIONS;
extern const inet::InitStage INITSTAGE_SIMU5G_REGISTRATIONS2;
extern const inet::InitStage INITSTAGE_SIMU5G_NODE_RELATIONSHIPS;
extern const inet::InitStage INITSTAGE_SIMU5G_BINDER_ACCESS;

extern const inet::InitStage INITSTAGE_SIMU5G_AMC_SETUP;
extern const inet::InitStage INITSTAGE_SIMU5G_PHYSICAL_LAYER;
extern const inet::InitStage INITSTAGE_SIMU5G_MAC_SCHEDULER_CREATION;
extern const inet::InitStage INITSTAGE_SIMU5G_BACKGROUNDTRAFFICMANAGER;
extern const inet::InitStage INITSTAGE_SIMU5G_TTI_SETUP;
extern const inet::InitStage INITSTAGE_SIMU5G_CELLINFO_CHANNELUPDATE;
extern const inet::InitStage INITSTAGE_SIMU5G_AMC_ATTACHUSER;
extern const inet::InitStage INITSTAGE_SIMU5G_SERVICE_PLATFORM_SETUP;

}

#endif
