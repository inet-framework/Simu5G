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
//
// Strict positive check (WR-05): ALL five fields must be present with the expected
// types and values. Missing fields, wrong types, wrong values, and out-of-range origins
// all return false — there is no defaulting to "identity" for absent fields.
//   axis_map   : string "identity"
//   scale      : number 1.0
//   origin     : array of exactly 3 numbers, each 0.0
//   units      : string "m"
//   handedness : string "right"
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

    // axis_map must be present and "identity"
    auto axisIt = ct.find("axis_map");
    if (axisIt == ct.end() || !axisIt->is_string() || axisIt->get<std::string>() != "identity")
        return false;

    // scale must be present and exactly 1.0
    auto scaleIt = ct.find("scale");
    if (scaleIt == ct.end() || !scaleIt->is_number() || scaleIt->get<double>() != 1.0)
        return false;

    // origin must be present, an array of exactly 3 elements, each the number 0.0
    auto originIt = ct.find("origin");
    if (originIt == ct.end() || !originIt->is_array() || originIt->size() != 3)
        return false;
    for (const auto& v : *originIt) {
        if (!v.is_number() || v.get<double>() != 0.0)
            return false;
    }

    // units must be present and "m" (not "km" or anything else)
    auto unitsIt = ct.find("units");
    if (unitsIt == ct.end() || !unitsIt->is_string() || unitsIt->get<std::string>() != "m")
        return false;

    // handedness must be present and "right"
    auto handIt = ct.find("handedness");
    if (handIt == ct.end() || !handIt->is_string() || handIt->get<std::string>() != "right")
        return false;

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

        // 2b) Optional cross-check: if componentCarrierModule is set, read the live
        // carrier/SCS/numBands values from the actual ComponentCarrier the PHY uses and
        // assert they match the shadow params above. A hand-copy divergence (e.g.
        // carrierFrequencyHz = 3.5e9 while the CC is configured to 2.6 GHz) aborts
        // the run loud here rather than producing a silently mismatched channel (CR-01).
        // When the param is empty (default) this cross-check is a no-op; existing users
        // are unaffected. TODO(phase-2): bind the contract directly to componentCarrier.
        const std::string ccPath = par("componentCarrierModule").stringValue();
        if (!ccPath.empty()) {
            cModule *ccMod = getModuleByPath(ccPath.c_str());
            if (ccMod == nullptr)
                throw cRuntimeError("SionnaManager: componentCarrierModule '%s' not found",
                                    ccPath.c_str());
            // carrierFrequency NED param is in GHz (unit annotation); doubleValue() returns
            // the value in the declared unit (GHz), so multiply by 1e9 to get Hz.
            double ccFreqHz = ccMod->par("carrierFrequency").doubleValue() * 1e9;
            int ccMu = ccMod->par("numerologyIndex").intValue();
            double ccScsHz = 15000.0 * (1 << ccMu);
            int ccNumBands = ccMod->par("numBands").intValue();

            if (std::fabs(live.carrier_frequency_hz - ccFreqHz) > 1e-6 * std::fabs(ccFreqHz))
                throw cRuntimeError("SionnaManager: carrierFrequencyHz (%g Hz) does not match "
                                    "componentCarrier '%s' carrierFrequency (%g Hz) — "
                                    "hand-copy divergence detected (CR-01)",
                                    live.carrier_frequency_hz, ccPath.c_str(), ccFreqHz);

            if (std::fabs(live.subcarrier_spacing_hz - ccScsHz) > 1e-6 * std::fabs(ccScsHz))
                throw cRuntimeError("SionnaManager: subcarrierSpacingHz (%g Hz) does not match "
                                    "componentCarrier '%s' numerologyIndex %d -> SCS %g Hz (CR-01)",
                                    live.subcarrier_spacing_hz, ccPath.c_str(), ccMu, ccScsHz);

            if (live.num_bands != ccNumBands)
                throw cRuntimeError("SionnaManager: numBands (%d) does not match "
                                    "componentCarrier '%s' numBands (%d) (CR-01)",
                                    live.num_bands, ccPath.c_str(), ccNumBands);
        }

        // 3) Assert the table encoding before reading: v1 targets little-endian float64
        //    ("<f8" in NumPy dtype notation). Reject anything else explicitly so a
        //    big-endian or integer-typed artifact fails loud rather than being silently
        //    byte-swapped or misinterpreted (IN-04).
        if (manifest_.table_dtype != "<f8")
            throw cRuntimeError("Sionna manifest table_dtype '%s' is not supported; "
                                "expected '<f8' (little-endian float64)",
                                manifest_.table_dtype.c_str());

        // 4) Load the bounds-validated binary path-gain table (resolved next to the
        //    manifest). No silent fallback anywhere above (CAL-02).
        table_ = SionnaTable::loadBinary(dirOf(manifestPath) + manifest_.table_path,
                                         manifest_.num_links);
    }
}

} //namespace
