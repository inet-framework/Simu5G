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

#include <omnetpp.h>

#include <climits>
#include <fstream>
#include <sstream>

// In-tree vendored single-header JSON parser (nlohmann/json 3.9.1, already shipped
// for the MEC HTTP utilities). Reused here rather than re-vendoring; it only compiles
// when the Simu5G_Sionna feature is ON (this folder is excluded from the default
// build), so it adds zero symbols to the default .so (SEAM-02 / threat T-03-SC).
#include "simu5g/mec/utils/httpUtils/json.hpp"

namespace simu5g {

using namespace omnetpp;
using json = nlohmann::json;

namespace {

// --- Strict typed extractors: throw cRuntimeError on missing key / type confusion
// (V5 input validation, T-03-02). No silent defaulting anywhere. ---

const json& requireKey(const json& j, const char *key)
{
    auto it = j.find(key);
    if (it == j.end())
        throw cRuntimeError("Sionna manifest missing required field '%s'", key);
    return *it;
}

int requireInt(const json& j, const char *key)
{
    const json& v = requireKey(j, key);
    // Accept integral JSON numbers only; reject floats/strings/bools (type confusion).
    if (!v.is_number_integer() && !v.is_number_unsigned())
        throw cRuntimeError("Sionna manifest field '%s' is not an integer", key);
    // Range-check before narrowing: get<long long>() is safe for both signed and
    // unsigned JSON integers, then assert it fits in int (rejects 2^32+1 etc.).
    long long raw = v.get<long long>();
    if (raw < (long long)INT_MIN || raw > (long long)INT_MAX)
        throw cRuntimeError("Sionna manifest field '%s' value %lld is out of int range",
                            key, raw);
    return (int)raw;
}

std::size_t requireSize(const json& j, const char *key)
{
    const json& v = requireKey(j, key);
    if (!v.is_number_unsigned() && !(v.is_number_integer() && v.get<long long>() >= 0))
        throw cRuntimeError("Sionna manifest field '%s' is not a non-negative integer", key);
    return (std::size_t)v.get<unsigned long long>();
}

double requireDouble(const json& j, const char *key)
{
    const json& v = requireKey(j, key);
    if (!v.is_number())
        throw cRuntimeError("Sionna manifest field '%s' is not a number", key);
    return v.get<double>();
}

std::string requireString(const json& j, const char *key)
{
    const json& v = requireKey(j, key);
    if (!v.is_string())
        throw cRuntimeError("Sionna manifest field '%s' is not a string", key);
    return v.get<std::string>();
}

// coord_transform is a nested object in the manifest (origin/axis_map/scale/units/
// handedness). Store it as its serialized JSON so SionnaManager can assert it against
// the live scenario (Phase 1: must be the identity transform).
std::string requireObjectAsString(const json& j, const char *key)
{
    const json& v = requireKey(j, key);
    if (!v.is_object())
        throw cRuntimeError("Sionna manifest field '%s' is not an object", key);
    return v.dump();
}

} // anonymous namespace

Manifest ManifestReader::read(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        throw cRuntimeError("Sionna manifest '%s' could not be opened", path.c_str());

    json j;
    try {
        in >> j;
    }
    catch (const json::exception& e) {
        throw cRuntimeError("Sionna manifest '%s' is not valid JSON: %s", path.c_str(), e.what());
    }
    if (!j.is_object())
        throw cRuntimeError("Sionna manifest '%s' top-level value is not a JSON object", path.c_str());

    Manifest m;
    m.schema_version        = requireInt(j, "schema_version");
    m.carrier_frequency_hz  = requireDouble(j, "carrier_frequency_hz");
    m.subcarrier_spacing_hz = requireDouble(j, "subcarrier_spacing_hz");
    m.num_bands             = requireInt(j, "num_bands");
    m.num_links             = requireSize(j, "num_links");
    m.table_path            = requireString(j, "table_path");
    m.table_dtype           = requireString(j, "table_dtype");
    m.coord_transform       = requireObjectAsString(j, "coord_transform");
    m.request_hash          = requireString(j, "request_hash");

    return m;
}

} //namespace
