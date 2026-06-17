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

#include "simu5g/stack/phy/channelmodel/sionna/SionnaManager.h"

#include <inet/common/InitStages.h>

namespace simu5g {

Define_Module(SionnaManager);

void SionnaManager::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        // TODO(01-03): read par("artifactManifest"), load the manifest + binary table
        // via ManifestReader/SionnaTable, then assert the parameter contract against
        // the live scenario and throw cRuntimeError on any mismatch (fail loud).
    }
}

} //namespace
