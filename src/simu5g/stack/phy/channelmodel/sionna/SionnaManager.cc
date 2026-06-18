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

#include <cmath>
#include <string>

#include "simu5g/mec/utils/httpUtils/json.hpp"

namespace simu5g {

using json = nlohmann::json;

Define_Module(SionnaManager);

namespace {

// Directory portion of a path (everything up to and including the last '/'), so a
// relative table_path in the manifest is resolved next to the manifest file.
std::string dirOf(const std::string& path)
{
    std::string::size_type slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return "";
    return path.substr(0, slash + 1);
}

// Phase 1 accepts ONLY the identity coord_transform (TOOL-02): the OMNeT++ scene
// frame and the Sionna scene frame must coincide, so the Euclidean distance funnel
// in getAttenuation is the Friis reference. Any other transform aborts (CAL-02).
bool isIdentityTransform(const std::string& coordTransformJson)
{
    json ct;
    try {
        ct = json::parse(coordTransformJson);
    }
    catch (const json::exception&) {
        return false;
    }
    if (!ct.is_object())
        return false;

    auto axisIt = ct.find("axis_map");
    if (axisIt == ct.end() || !axisIt->is_string() || axisIt->get<std::string>() != "identity")
        return false;

    auto scaleIt = ct.find("scale");
    if (scaleIt != ct.end() && scaleIt->is_number() && scaleIt->get<double>() != 1.0)
        return false;

    auto originIt = ct.find("origin");
    if (originIt != ct.end() && originIt->is_array()) {
        for (const auto& v : *originIt) {
            if (!v.is_number() || v.get<double>() != 0.0)
                return false;
        }
    }
    return true;
}

} // anonymous namespace

void SionnaManager::assertContractMatchesLiveScenario(const Manifest& m, const LiveContract& live)
{
    // Pitfall 5: assert EVERY contract field, not just schema_version; abort on the
    // first mismatch with a distinct cRuntimeError. No silent fallback (CAL-02).
    if (m.schema_version != EXPECTED_SCHEMA_VERSION)
        throw cRuntimeError("Sionna manifest schema_version %d != expected %d",
                            m.schema_version, EXPECTED_SCHEMA_VERSION);

    // Use a relative tolerance (1 ppm) for floating-point contract values: the manifest
    // carries JSON text (e.g. 3.5e9) while the live value comes from NED unit parsing
    // ("3.5GHz"), and different representations of the same logical value can differ by
    // a ULP. Exact != is brittle and can abort a legitimately-matching run (WR-04).
    // num_bands and schema_version stay as exact integer compares.
    if (std::fabs(m.carrier_frequency_hz - live.carrier_frequency_hz) >
            1e-6 * std::fabs(live.carrier_frequency_hz))
        throw cRuntimeError("Sionna manifest carrier_frequency_hz mismatch: artifact %g Hz, "
                            "scenario %g Hz", m.carrier_frequency_hz, live.carrier_frequency_hz);

    if (std::fabs(m.subcarrier_spacing_hz - live.subcarrier_spacing_hz) >
            1e-6 * std::fabs(live.subcarrier_spacing_hz))
        throw cRuntimeError("Sionna manifest subcarrier_spacing_hz mismatch: artifact %g Hz, "
                            "scenario %g Hz", m.subcarrier_spacing_hz, live.subcarrier_spacing_hz);

    if (m.num_bands != live.num_bands)
        throw cRuntimeError("Sionna manifest num_bands mismatch: artifact %d, scenario %d",
                            m.num_bands, live.num_bands);

    if (!isIdentityTransform(m.coord_transform))
        throw cRuntimeError("Sionna manifest coord_transform is not the identity transform "
                            "(Phase 1 supports identity only): %s", m.coord_transform.c_str());

    // request_hash is producer-side provenance (identifies the precompute run that
    // generated the artifact); the C++ consumer does not recompute or verify it for
    // integrity — it only requires the field to be present and non-empty so a manifest
    // truncated before the hash line is rejected (WR-03 / IN-05).
    if (m.request_hash.empty())
        throw cRuntimeError("Sionna manifest request_hash is empty (producer provenance field required)");
}

void SionnaManager::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        const std::string manifestPath = par("artifactManifest").stringValue();

        // 1) Parse + validate the manifest JSON (V5 input validation inside the reader).
        manifest_ = ManifestReader::read(manifestPath);

        // 2) Fail-loud contract assertion against this manager's configured live values.
        LiveContract live;
        live.carrier_frequency_hz = par("carrierFrequencyHz").doubleValue();
        live.subcarrier_spacing_hz = par("subcarrierSpacingHz").doubleValue();
        live.num_bands = par("numBands").intValue();
        assertContractMatchesLiveScenario(manifest_, live);

        // 3) Load the bounds-validated binary path-gain table (resolved next to the
        //    manifest). No silent fallback anywhere above (CAL-02).
        table_ = SionnaTable::loadBinary(dirOf(manifestPath) + manifest_.table_path,
                                         manifest_.num_links);
    }
}

} //namespace
