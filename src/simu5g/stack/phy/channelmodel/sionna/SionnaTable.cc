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

#include "simu5g/stack/phy/channelmodel/sionna/SionnaTable.h"

namespace simu5g {

SionnaTable SionnaTable::loadBinary(const std::string& path, std::size_t numLinks)
{
    // TODO(01-03): open `path` with std::ifstream(std::ios::binary), validate the
    // declared length (reject negative/oversized), read numLinks float64 values into
    // pathGainDb_, and throw cRuntimeError on any malformed input.
    (void)path;
    (void)numLinks;
    return SionnaTable();
}

double SionnaTable::lookup(std::size_t linkIndex) const
{
    // TODO(01-03): bounds-checked lookup; throw on out-of-range linkIndex.
    (void)linkIndex;
    return 0.0;
}

} //namespace
