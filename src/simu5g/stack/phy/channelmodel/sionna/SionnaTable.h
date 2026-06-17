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

#ifndef SIONNATABLE_H_
#define SIONNATABLE_H_

#include <cstddef>
#include <string>
#include <vector>

namespace simu5g {

//
// Plain in-memory holder for the precomputed Sionna path-gain table. v1 is
// noise-limited: one path gain (dB) per link, indexed by a validated link key. The
// schema is v2-ready (a degenerate SINR-bin axis collapses to this scalar slice).
//
// This is a compilable skeleton; the bounds-validated binary loader and lookup are
// filled in by Plan 01-03. No Sionna/HDF5/Python headers are pulled in here.
//
class SionnaTable
{
  protected:
    // Per-link path gain in dB, indexed by link key [L].
    std::vector<double> pathGainDb_;

  public:
    /*
     * Load a little-endian float64 [L] path-gain table from a binary artifact.
     * The read length is bounded by the declared numLinks (input validation).
     *
     * @param path path to the binary table file
     * @param numLinks number of links declared by the manifest
     * @return the populated table
     */
    static SionnaTable loadBinary(const std::string& path, std::size_t numLinks);

    /*
     * @param linkIndex validated link key
     * @return path gain (dB) for the link
     */
    double lookup(std::size_t linkIndex) const;

    /*
     * @return number of links held by the table.
     */
    std::size_t size() const { return pathGainDb_.size(); }
};

} //namespace

#endif /* SIONNATABLE_H_ */
