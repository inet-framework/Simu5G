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

#ifndef MANIFESTREADER_H_
#define MANIFESTREADER_H_

#include <cstddef>
#include <string>

namespace simu5g {

//
// Parsed contents of the Sionna artifact manifest (the small JSON sidecar carrying the
// parameter contract + request hash). The bulk numeric path-gain table lives in a
// separate little-endian binary file referenced by table_path.
//
// This is a compilable skeleton; the JSON parsing and validation are filled in by
// Plan 01-03. No Sionna/HDF5/Python headers are pulled in here.
//
struct Manifest {
    int schema_version = 0;
    double carrier_frequency_hz = 0.0;
    double subcarrier_spacing_hz = 0.0;
    int num_bands = 0;
    std::string table_path;
    std::string table_dtype;
    std::size_t num_links = 0;
    // coord_transform placeholder: the TOOL-02 coordinate transform between the
    // OMNeT++ scene frame and the Sionna scene frame (filled in by Plan 01-03).
    std::string coord_transform;
    // request_hash placeholder: hash of the full request (scene, positions, materials,
    // antennas, freqs, powers, MCS set) used as the cache key (filled in by Plan 01-03).
    std::string request_hash;
};

//
// Reads and validates a Sionna manifest JSON file into a Manifest struct.
//
class ManifestReader
{
  public:
    /*
     * Read and parse the manifest JSON at `path`.
     *
     * @param path path to manifest.json
     * @return the parsed manifest
     */
    static Manifest read(const std::string& path);
};

} //namespace

#endif /* MANIFESTREADER_H_ */
