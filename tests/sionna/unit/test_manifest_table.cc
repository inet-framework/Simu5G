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

// Standalone unit harness for ManifestReader + SionnaTable (Plan 01-03 Task 1).
//
// It compiles the real ManifestReader.cc / SionnaTable.cc against liboppsim only
// (no INET / no Simu5G kernel) and exercises the V5 input-validation contract:
// well-formed parse populates the struct; malformed/oversized/typed-wrong input
// throws cRuntimeError. cStaticFlag makes cRuntimeError catchable outside a run.
//
// Build/run via tests/sionna/unit/run_unit_tests.sh.

#include <omnetpp.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "simu5g/stack/phy/channelmodel/sionna/ManifestReader.h"
#include "simu5g/stack/phy/channelmodel/sionna/SionnaTable.h"

using namespace omnetpp;
using namespace simu5g;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(cond)) {                                                          \
            ++g_failures;                                                       \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__);      \
        }                                                                       \
    } while (0)

// Returns true if calling fn() throws cRuntimeError.
template<typename F>
static bool throwsRuntimeError(F fn)
{
    try {
        fn();
    }
    catch (const cRuntimeError&) {
        return true;
    }
    catch (...) {
        return false;
    }
    return false;
}

static std::string writeTempFile(const std::string& name, const std::string& content)
{
    std::string path = std::string("/tmp/") + name;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(content.data(), (std::streamsize)content.size());
    f.close();
    return path;
}

static std::string writeBinaryDoubles(const std::string& name, const std::vector<double>& vals)
{
    std::string path = std::string("/tmp/") + name;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(vals.data()),
            (std::streamsize)(vals.size() * sizeof(double)));
    f.close();
    return path;
}

// A well-formed manifest matching the keys written by Plan 01-01 (precompute.py).
static const char *kGoodManifest = R"JSON({
  "schema_version": 1,
  "carrier_frequency_hz": 3.5e9,
  "subcarrier_spacing_hz": 30000.0,
  "num_bands": 1,
  "num_links": 1,
  "table_path": "path_gain.bin",
  "table_dtype": "<f8",
  "coord_transform": { "origin": [0.0, 0.0, 0.0], "axis_map": "identity",
                       "scale": 1.0, "units": "m", "handedness": "right" },
  "request_hash": "deadbeefcafef00d",
  "sinr_grid": [0.0]
})JSON";

int main()
{
    cStaticFlag dummy;  // makes cRuntimeError catchable instead of abort()ing

    // --- ManifestReader: well-formed parse populates every contract field ---
    {
        std::string p = writeTempFile("sionna_good_manifest.json", kGoodManifest);
        Manifest m = ManifestReader::read(p);
        CHECK(m.schema_version == 1, "schema_version parsed");
        CHECK(m.carrier_frequency_hz == 3.5e9, "carrier_frequency_hz parsed");
        CHECK(m.subcarrier_spacing_hz == 30000.0, "subcarrier_spacing_hz parsed");
        CHECK(m.num_bands == 1, "num_bands parsed");
        CHECK(m.num_links == 1, "num_links parsed");
        CHECK(m.table_path == "path_gain.bin", "table_path parsed");
        CHECK(m.table_dtype == "<f8", "table_dtype parsed");
        CHECK(!m.coord_transform.empty(), "coord_transform parsed (non-empty)");
        CHECK(m.request_hash == "deadbeefcafef00d", "request_hash parsed");
    }

    // --- ManifestReader: missing required key -> cRuntimeError ---
    {
        std::string bad = R"JSON({ "carrier_frequency_hz": 3.5e9 })JSON";
        std::string p = writeTempFile("sionna_missing_key.json", bad);
        CHECK(throwsRuntimeError([&] { ManifestReader::read(p); }),
              "missing schema_version throws cRuntimeError");
    }

    // --- ManifestReader: type confusion (string where number expected) -> throw ---
    {
        std::string bad = R"JSON({
          "schema_version": 1, "carrier_frequency_hz": "not-a-number",
          "subcarrier_spacing_hz": 30000.0, "num_bands": 1, "num_links": 1,
          "table_path": "path_gain.bin", "table_dtype": "<f8",
          "coord_transform": {}, "request_hash": "ab" })JSON";
        std::string p = writeTempFile("sionna_typed_wrong.json", bad);
        CHECK(throwsRuntimeError([&] { ManifestReader::read(p); }),
              "type confusion throws cRuntimeError");
    }

    // --- ManifestReader: malformed JSON / nonexistent file -> throw ---
    {
        std::string p = writeTempFile("sionna_malformed.json", "{ not json ");
        CHECK(throwsRuntimeError([&] { ManifestReader::read(p); }),
              "malformed JSON throws cRuntimeError");
        CHECK(throwsRuntimeError([&] { ManifestReader::read("/tmp/does-not-exist-xyz.json"); }),
              "missing file throws cRuntimeError");
    }

    // --- SionnaTable: well-formed binary loads and looks up ---
    {
        std::string p = writeBinaryDoubles("sionna_good.bin", { -83.36, -90.1, -100.0 });
        SionnaTable t = SionnaTable::loadBinary(p, 3);
        CHECK(t.size() == 3, "loadBinary populated 3 links");
        CHECK(t.lookup(0) == -83.36, "lookup(0) correct");
        CHECK(t.lookup(2) == -100.0, "lookup(2) correct");
    }

    // --- SionnaTable: file size mismatch (declared num_links wrong) -> throw ---
    {
        std::string p = writeBinaryDoubles("sionna_sizemismatch.bin", { -83.36, -90.1 });
        CHECK(throwsRuntimeError([&] { SionnaTable::loadBinary(p, 3); }),
              "fileSize != num_links*8 throws cRuntimeError");
    }

    // --- SionnaTable: num_links == 0 rejected before allocation ---
    {
        std::string p = writeBinaryDoubles("sionna_empty.bin", {});
        CHECK(throwsRuntimeError([&] { SionnaTable::loadBinary(p, 0); }),
              "num_links==0 throws cRuntimeError");
    }

    // --- SionnaTable: oversized num_links rejected before allocation ---
    {
        std::string p = writeBinaryDoubles("sionna_one.bin", { -1.0 });
        CHECK(throwsRuntimeError([&] { SionnaTable::loadBinary(p, (std::size_t)1 << 40); }),
              "oversized num_links throws cRuntimeError");
    }

    // --- SionnaTable: lookup out-of-range -> throw ---
    {
        std::string p = writeBinaryDoubles("sionna_lk.bin", { -10.0 });
        SionnaTable t = SionnaTable::loadBinary(p, 1);
        CHECK(throwsRuntimeError([&] { (void)t.lookup(5); }),
              "out-of-range lookup throws cRuntimeError");
    }

    std::fprintf(stderr, "\n%d/%d checks passed\n", g_checks - g_failures, g_checks);
    if (g_failures) {
        std::fprintf(stderr, "TEST RESULT: FAIL (%d failures)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "TEST RESULT: PASS\n");
    return 0;
}
