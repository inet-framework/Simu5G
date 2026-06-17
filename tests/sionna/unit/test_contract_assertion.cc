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

// Standalone unit harness for SionnaManager::assertContractMatchesLiveScenario
// (Plan 01-03 Task 2, CAL-02 / Pitfall 5). The assertion is pure (no module / no
// par() access), so it links only against liboppsim and runs without the kernel.
//
// Built/run via tests/sionna/unit/run_unit_tests.sh.

#include <omnetpp.h>

#include <cstdio>

#include "simu5g/stack/phy/channelmodel/sionna/ManifestReader.h"
#include "simu5g/stack/phy/channelmodel/sionna/SionnaManager.h"

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

template<typename F>
static bool throwsRuntimeError(F fn)
{
    try { fn(); }
    catch (const cRuntimeError&) { return true; }
    catch (...) { return false; }
    return false;
}

// A manifest that fully matches the canonical live scenario below.
static Manifest goodManifest()
{
    Manifest m;
    m.schema_version = 1;
    m.carrier_frequency_hz = 3.5e9;
    m.subcarrier_spacing_hz = 30000.0;
    m.num_bands = 1;
    m.num_links = 1;
    m.table_path = "path_gain.bin";
    m.table_dtype = "<f8";
    m.coord_transform = R"({"axis_map":"identity","handedness":"right","origin":[0.0,0.0,0.0],"scale":1.0,"units":"m"})";
    m.request_hash = "deadbeefcafef00d";
    return m;
}

static SionnaManager::LiveContract goodLive()
{
    SionnaManager::LiveContract live;
    live.carrier_frequency_hz = 3.5e9;
    live.subcarrier_spacing_hz = 30000.0;
    live.num_bands = 1;
    return live;
}

int main()
{
    cStaticFlag dummy;

    // --- Matching manifest passes (no throw) ---
    {
        Manifest m = goodManifest();
        bool threw = throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        });
        CHECK(!threw, "matching contract does not throw");
    }

    // --- schema_version mismatch -> throw ---
    {
        Manifest m = goodManifest();
        m.schema_version = 2;
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "schema_version mismatch throws");
    }

    // --- carrier frequency mismatch -> throw ---
    {
        Manifest m = goodManifest();
        m.carrier_frequency_hz = 2.6e9;
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "carrier_frequency mismatch throws");
    }

    // --- subcarrier spacing mismatch -> throw ---
    {
        Manifest m = goodManifest();
        m.subcarrier_spacing_hz = 15000.0;
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "subcarrier_spacing mismatch throws");
    }

    // --- band count mismatch -> throw ---
    {
        Manifest m = goodManifest();
        m.num_bands = 6;
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "num_bands mismatch throws");
    }

    // --- non-identity coord_transform -> throw ---
    {
        Manifest m = goodManifest();
        m.coord_transform = R"({"axis_map":"swap_xy","origin":[0.0,0.0,0.0],"scale":1.0})";
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "non-identity coord_transform throws");
    }

    // --- empty request_hash -> throw ---
    {
        Manifest m = goodManifest();
        m.request_hash = "";
        CHECK(throwsRuntimeError([&] {
            SionnaManager::assertContractMatchesLiveScenario(m, goodLive());
        }), "empty request_hash throws");
    }

    std::fprintf(stderr, "\n%d/%d checks passed\n", g_checks - g_failures, g_checks);
    if (g_failures) {
        std::fprintf(stderr, "TEST RESULT: FAIL (%d failures)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "TEST RESULT: PASS\n");
    return 0;
}
