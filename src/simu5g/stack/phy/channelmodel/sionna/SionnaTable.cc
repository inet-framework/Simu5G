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

#include <omnetpp.h>

#include <fstream>
#include <ios>

namespace simu5g {

using namespace omnetpp;

// Sanity cap on the declared number of links: rejects integer-overflow / absurd
// header values before any allocation (V5 input validation, T-03-01). 2^24 links
// is far beyond any realistic v1 scenario yet bounds the read to ~128 MiB.
static const std::size_t kMaxLinks = (std::size_t)1 << 24;

SionnaTable SionnaTable::loadBinary(const std::string& path, std::size_t numLinks)
{
    // Reject degenerate / oversized declared lengths BEFORE allocating (T-03-01).
    if (numLinks == 0)
        throw cRuntimeError("Sionna path-gain table '%s': declared num_links is 0 (empty table)",
                            path.c_str());
    if (numLinks > kMaxLinks)
        throw cRuntimeError("Sionna path-gain table '%s': declared num_links %lu exceeds cap %lu",
                            path.c_str(), (unsigned long)numLinks, (unsigned long)kMaxLinks);

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
        throw cRuntimeError("Sionna path-gain table '%s' could not be opened", path.c_str());

    // Seek-to-end (std::ios::ate) gives the file size; validate it against the
    // declared length so a tampered/truncated table can never over-read (T-03-01).
    const std::streamoff fileSize = f.tellg();
    if (fileSize < 0)
        throw cRuntimeError("Sionna path-gain table '%s': could not determine file size", path.c_str());

    const std::streamoff expectedSize = (std::streamoff)(numLinks * sizeof(double));
    if (fileSize != expectedSize)
        throw cRuntimeError("Sionna path-gain table '%s': file size %lld bytes != "
                            "num_links(%lu) * sizeof(double)(%lu) = %lld bytes",
                            path.c_str(), (long long)fileSize, (unsigned long)numLinks,
                            (unsigned long)sizeof(double), (long long)expectedSize);

    SionnaTable t;
    t.pathGainDb_.resize(numLinks);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char *>(t.pathGainDb_.data()), expectedSize);
    if (!f || f.gcount() != expectedSize)
        throw cRuntimeError("Sionna path-gain table '%s': short read (%lld of %lld bytes)",
                            path.c_str(), (long long)f.gcount(), (long long)expectedSize);

    return t;
}

double SionnaTable::lookup(std::size_t linkIndex) const
{
    if (linkIndex >= pathGainDb_.size())
        throw cRuntimeError("Sionna path-gain lookup: link index %lu out of range [0, %lu)",
                            (unsigned long)linkIndex, (unsigned long)pathGainDb_.size());
    return pathGainDb_[linkIndex];
}

} //namespace
