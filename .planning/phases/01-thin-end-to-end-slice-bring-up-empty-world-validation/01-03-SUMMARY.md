---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
plan: 03
subsystem: sionna-channelmodel-consumer
tags: [sionna, cpp, channelmodel, artifact-load, contract-assertion, input-validation, tdd, omnetpp]
dependency_graph:
  requires:
    - "Plan 01-01: manifest.json schema + path_gain.bin LE float64 [L] table layout"
    - "Plan 01-02: compilable C++ skeletons (ManifestReader, SionnaTable, SionnaManager, SionnaChannelModel) + Simu5G_Sionna build isolation"
  provides:
    - "ManifestReader::read — strict typed JSON manifest parse (nlohmann/json 3.9.1, in-tree) with fail-loud cRuntimeError on missing key / type confusion"
    - "SionnaTable::loadBinary — bounds-validated LE float64 table read (fileSize == num_links*sizeof(double); rejects 0/oversized); bounds-checked lookup"
    - "SionnaManager::assertContractMatchesLiveScenario — fail-loud assertion of schema_version + every contract field (CAL-02)"
    - "SionnaChannelModel::getAttenuation — returns -table.lookup(linkKey) while reusing the entire inherited SINR/RSRP pipeline (MOD-01)"
    - "tests/sionna/unit standalone unit harness (liboppsim/libINET only, no kernel) — 27 checks"
  affects:
    - "Plan 01-04: feature-ON build compile + wrong-manifest-aborts-run integration gate; SEAM-02 symbol check"
tech_stack:
  added: []
  patterns:
    - "Reuse the in-tree vendored nlohmann/json.hpp 3.9.1 (src/simu5g/mec/utils/httpUtils) — compiled only when Simu5G_Sionna is ON, zero default-build symbols (T-03-SC)"
    - "Pure static contract-assertion (no module/par access) keeps fail-loud logic unit-testable without the OMNeT++ kernel"
    - "Standalone C++ unit harness linking liboppsim (+libINET) with cStaticFlag so cRuntimeError is catchable outside a simulation run"
key_files:
  created:
    - "tests/sionna/unit/test_manifest_table.cc"
    - "tests/sionna/unit/test_contract_assertion.cc"
    - "tests/sionna/unit/run_unit_tests.sh"
  modified:
    - "src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.cc"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.h"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.ned"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h"
    - "src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc"
decisions:
  - "Reused the existing in-tree nlohmann/json 3.9.1 (MEC httpUtils) for manifest parsing instead of vendoring a new header — no package install, no new file, honors threat T-03-SC; included via the full src-relative path so it resolves under the real opp_makemake build (INCLUDE_PATH=-Isrc)."
  - "Added carrierFrequencyHz/subcarrierSpacingHz/numBands NED params to SionnaManager so the live-scenario contract is configured at the single assertion point (the manager is not a channel model and has no inherited carrier members)."
  - "Split the fail-loud contract check into a pure static assertContractMatchesLiveScenario(Manifest, LiveContract) so it is unit-testable without a running module; initialize() reads par() then delegates to it."
  - "Built a standalone C++ unit harness (tests/sionna/unit) linking only liboppsim/libINET with cStaticFlag — enables real RED/GREEN TDD for the pure utilities without the full simulation kernel."
metrics:
  duration_min: 8
  completed: "2026-06-17"
  tasks: 3
  files: 10
---

# Phase 1 Plan 03: Sionna artifact consumer (load + contract assertion + path-gain substitution) Summary

Implemented the C++ consumer half of the walking skeleton: a strictly-validated `ManifestReader` (nlohmann/json 3.9.1, reused in-tree) and a bounds-checked `SionnaTable` binary loader; a `SionnaManager` that loads the artifact at `INITSTAGE_LOCAL` and fail-loudly asserts schema_version plus every contract field against the live scenario (CAL-02, no silent fallback); and a `SionnaChannelModel::getAttenuation` override that returns the negated Sionna path gain from the table while reusing the entire inherited SINR/RSRP/interference pipeline unchanged (MOD-01). A standalone unit harness exercises the pure logic with **27/27** checks passing.

## What Was Built

| Task | Description | RED | GREEN |
|------|-------------|-----|-------|
| 1 | ManifestReader (typed JSON parse, fail-loud) + SionnaTable (validated LE-binary loader, bounds-checked lookup) | `dee91e0f` (0/20) | `9237c45a` (20/20) |
| 2 | SionnaManager fail-loud contract assertion at INITSTAGE_LOCAL (CAL-02) | `c7ad0e21` (method absent in stub) | `9e403360` (7/7) |
| 3 | SionnaChannelModel::getAttenuation returns -table lookup, reuses inherited SINR (MOD-01) | — (module-bound; see TDD note) | `87a648a2` |

## Verification Evidence

- **Task 1 gate** `READER-OK`: ManifestReader.cc + SionnaTable.cc both throw cRuntimeError; SionnaTable validates `sizeof(double)`; ManifestReader reads `schema_version`.
- **Task 2 gate** `MANAGER-OK`: SionnaManager.cc gated on `INITSTAGE_LOCAL`, defines `EXPECTED_SCHEMA_VERSION`, reads `schema_version`, and has **6** distinct `cRuntimeError` throws (≥2 required, Pitfall 5 satisfied).
- **Task 3 gate** `OVERRIDE-OK`: getAttenuation keeps `getCoord().distance(coord)`, calls `lookup`, chains `NrChannelModel::initialize`, and does NOT call `computePathLoss`.
- **Unit harness** (`tests/sionna/unit/run_unit_tests.sh`): `test_manifest_table` **20/20** + `test_contract_assertion` **7/7** = **27/27** PASS, exit 0. Compiles the real `.cc` files against liboppsim/libINET.

## Requirements Exercised

- **ART-01** (load side): manifest.json parsed + path_gain.bin LE float64 table read into memory; keys match those written by Plan 01-01.
- **MOD-01**: `getAttenuation` substitutes the Sionna path gain (negated table lookup) through the existing seam; no SINR/interference/noise math reimplemented.
- **CAL-02**: schema_version + every contract field (carrier freq, SCS, band count, identity coord_transform, non-empty request_hash) asserted at init; first mismatch aborts with cRuntimeError; no silent fallback.
- **V5** (input validation): typed JSON parse throws on missing key / type confusion; binary read validates fileSize against `num_links*sizeof(double)`, rejects `num_links==0` and `num_links > kMaxLinks` before allocation; lookup is bounds-checked.

## TDD Gate Compliance

- **Task 1**: genuine RED (`test(01-03)` `dee91e0f`, **0/20** against 01-02 stubs) -> GREEN (`feat(01-03)` `9237c45a`, **20/20**). RED ran and failed before implementation.
- **Task 2**: RED (`test(01-03)` `c7ad0e21`) -> GREEN (`feat(01-03)` `9e403360`, **7/7**). The RED test references `SionnaManager::assertContractMatchesLiveScenario`, which did not exist in the 01-02 stub (does not compile/link) — a genuine RED state.
- **Task 3**: `getAttenuation` is module-bound (requires `phy_`, `par()`, a live `SionnaManager`), so it is not exercisable in the standalone harness. Its load-bearing testable invariant — the bounds-checked table lookup and the negation sign convention — is covered by the Task 1 SionnaTable unit tests; the override wiring (initialize chaining, manager resolution, `-lookup` return) is verified statically by the `OVERRIDE-OK` gate and is compiled+run by the Plan 01-04 feature-ON integration build (the canonical end-to-end gate for this override). No source-only RED was authored for Task 3 because no kernel-free unit boundary exists for it.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] SionnaManager had no live-scenario contract source**
- **Found during:** Task 2.
- **Issue:** The plan says SionnaManager reads expected carrier_frequency_hz / subcarrier_spacing_hz / num_bands "from this module's own parameters," but the 01-02 skeleton NED declared no such params (SionnaManager is a plain `cSimpleModule`, not a channel model with inherited `carrierFrequencyHz_`/`numBands_`).
- **Fix:** Added `double carrierFrequencyHz`, `double subcarrierSpacingHz`, `int numBands` parameters to `SionnaManager.ned`; `initialize` reads them into a `LiveContract` passed to the pure assertion. This realizes the plan's stated design ("read from this module's own parameters") literally.
- **Files modified:** `SionnaManager.ned`, `SionnaManager.h`, `SionnaManager.cc`.
- **Commit:** `9e403360`.

**2. [Rule 3 - Blocking] No JSON parser existed for ManifestReader**
- **Found during:** Task 1.
- **Issue:** The plan allows either OMNeT++ `cValueMap`/`cValueArray` or a vendored single-header JSON lib. No JSON-file parser is wired in-tree for the channel models.
- **Fix:** Discovered an already-vendored `nlohmann/json.hpp` 3.9.1 at `src/simu5g/mec/utils/httpUtils/json.hpp` (used by MEC) and reused it via its full src-relative include path. This adds **no new file**, **no package install**, and — because the sionna folder only compiles when `Simu5G_Sionna` is ON — **zero symbols to the default build** (honors threat T-03-SC). Strictly better than re-vendoring a duplicate header.
- **Files modified:** `ManifestReader.cc` (include + parse logic).
- **Commit:** `9237c45a`.

### Threat Model

All `mitigate` dispositions implemented:
- **T-03-01** (binary table tampering/DoS): `SionnaTable::loadBinary` validates `fileSize == num_links*sizeof(double)`, rejects `num_links==0` and `> kMaxLinks (2^24)` before allocating, and bounds-checks `lookup`. (Unit-verified: size-mismatch, zero, oversized, out-of-range lookup all throw.)
- **T-03-02** (manifest tampering): strict typed extractors throw `cRuntimeError` on missing key / type confusion / malformed JSON / missing file. (Unit-verified.)
- **T-03-03** (mismatched artifact silently accepted): `SionnaManager` asserts schema_version + every contract field at init, aborting on first mismatch; no silent fallback. (Unit-verified across 6 mismatch cases.)
- **T-03-04 / T-03-SC** (`accept`): artifactManifest path treated as trusted local research config; the JSON header is reused in-tree (no fetch, no install).

## Authentication Gates

None.

## Known Stubs

| File | Stub | Reason / Resolved by |
|------|------|----------------------|
| `SionnaChannelModel.cc` | `linkKeyFor(...)` returns 0 | Intentional v1 scope: a single empty-world Tx/Rx link. v2 maps the (Tx, Rx) pair to its precomputed table row. Documented inline. |

The `linkKeyFor` single-link return is the documented v1 design (one link), not an incomplete implementation — the table, manifest, and `getAttenuation` are all `[L]`-shaped and ready for the v2 multi-link key.

## Deferred Items (out of scope)

- **Stale generated `_m.h` headers** (`src/simu5g/common/LteCommonEnum_m.h`, `LteCommon_m.h`) report an opp_msgtool version mismatch on a from-scratch `g++ -fsyntax-only` of `SionnaChannelModel.cc`. This is pre-existing build-tree staleness (regenerated by `make`), unrelated to this plan's changes, and is resolved by the normal `make` performed in Plan 01-04's feature-ON build. Not fixed here (scope boundary). The pure utilities compile+run cleanly in the standalone harness, which does not include the generated headers.

## Threat Flags

None — no new network/auth surface. The C++ side only reads two local files (manifest.json + path_gain.bin), both treated as untrusted input and fully validated.

## Self-Check: PASSED

All 3 created files and 7 modified files exist on disk; all 5 task commits (`dee91e0f`, `9237c45a`, `c7ad0e21`, `9e403360`, `87a648a2`) are present in git history. Unit harness re-run: 27/27 checks pass, exit 0.

---
*Phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation*
*Completed: 2026-06-17*
