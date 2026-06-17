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

#include "simu5g/stack/phy/channelmodel/sionna/ManifestReader.h"

namespace simu5g {

Manifest ManifestReader::read(const std::string& path)
{
    // TODO(01-03): open `path`, parse the manifest JSON (cValueMap/cValueArray or a
    // vendored single-header JSON lib compiled only under Simu5G_Sionna), validate every
    // contract field, and throw cRuntimeError on missing keys / type confusion.
    (void)path;
    return Manifest();
}

} //namespace
