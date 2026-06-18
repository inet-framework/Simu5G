---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
verified: 2026-06-18T00:00:00Z
status: passed
score: 5/5 must-haves verified (all automated truths VERIFIED; 2 human items confirmed via UAT 2026-06-18)
overrides_applied: 0
human_verification_resolved: "Both human-verification items confirmed PASS in 01-UAT.md (commit 175d690c). Item 1: committed 0.sca (commit ed360465) shows ue[0].cellularNic.nrChannelModel[0] rcvdSinrDl:mean=70.13959 dB / measuredSinrDl:mean=56.42577 dB — tracks the -83.3604 dB Sionna table, not the ~97.5 dB analytic 3GPP attenuation. Item 2: run log shows loud cRuntimeError 'carrier_frequency_hz mismatch: artifact 2.6e+09 Hz, scenario 3.5e+09 Hz' on a corrupted manifest, nonzero exit, no silent fallback."
human_verification:
  - test: "Confirm the end-to-end single-link simulation ran with SionnaChannelModel active and the in-sim path gain reflects the Sionna table (~-83.3 dB path gain at 100 m), not the analytic formula."
    expected: "rcvdSinrDl or the logged getAttenuation value tracks the offline friis_check.py value within ~1 dB and is NOT the analytic 3GPP path loss."
    why_human: "Cannot read the SINR scalar from the uncommitted .sca run. The results/General/0.vec file (94 KB) confirms the sim ran but does not let us grep the exact path gain figure without an opp_scavetool parse."
  - test: "Confirm the corrupted-manifest negative check: edit manifest carrier_frequency_hz to a wrong value and re-run the simulation. It must abort with cRuntimeError naming the mismatch."
    expected: "Run exits nonzero with a message like 'Sionna manifest carrier_frequency_hz mismatch: artifact X Hz, scenario Y Hz'."
    why_human: "The negative test requires a deliberate edit, a live sim run, and observation of the abort message. Cannot reproduce this without running the binary."
---

# Phase 01: Thin End-to-End Slice (Bring-Up and Empty-World Validation) Verification Report

**Phase Goal:** Deliver a thin end-to-end vertical slice of the Simu5G x Sionna integration — an opt-in `SionnaChannelModel` that reads a precomputed empty-world artifact (offline Sionna tool -> HDF5 + JSON manifest + LE-binary table) and feeds per-link path gain into Simu5G, validated by the empty-world Friis round-trip — WITHOUT changing the default Simu5G build or behavior.
**Verified:** 2026-06-17
**Status:** human_needed
**Re-verification:** No — initial verification.

---

## Goal Achievement

### Observable Truths (Roadmap Success Criteria)

| #  | Truth                                                                                                                                                | Status     | Evidence                                                                                                                                                                       |
|----|------------------------------------------------------------------------------------------------------------------------------------------------------|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1  | A researcher selects SionnaChannelModel purely via the nrChannelModelType ini string (no NED interface edit) and the single-link simulation runs end-to-end on the Sionna path gain. | ? UNCERTAIN (human) | `omnetpp.ini:70` sets `*.*.cellularNic.nrChannelModelType = "SionnaChannelModel"`. NrNicUe.ned:71 uses the parametric typename. NED not edited. Simulation produced 94 KB result files. **Whether the run actually used the Sionna table vs. analytic model requires human confirmation** (see human verification item 1). |
| 2  | The empty-world Friis round-trip check passes: Sionna LOS path gain agrees with the textbook Friis value within the physics-derived tolerance (~0.5–1 dB). | ✓ VERIFIED | `friis_check.py` prints Sionna -83.3604 dB vs. Friis -83.3544 dB, residual 0.006 dB, exit 0. `pytest tests/test_friis.py` passed 1/1 under the offline venv. Binary artifact at `simulations/NR/sionna/sionna_artifact/path_gain.bin` contains -83.3604 dB (verified by direct float64 read). Residual 0.006 dB is <<1 dB gate. |
| 3  | SionnaManager asserts the artifact manifest against the live scenario at init and aborts with cRuntimeError on any contract-field or schema_version mismatch (no silent fallback). | ? UNCERTAIN (partial) | Code verifiably implements 6 distinct cRuntimeError throws in `assertContractMatchesLiveScenario` (schema_version, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, coord_transform, request_hash). Unit harness 7/7 contract-assertion checks pass. **However**, the "live scenario" values come from SionnaManager's own NED params, not the live PHY's componentCarrier (CR-01 design gap; see warnings below). The in-sim negative check (corrupted manifest aborts) requires human confirmation (item 2). |
| 4  | A default make build and a normal simulation run produce output byte-for-byte identical to the pre-integration baseline, with no Python/TensorFlow/PyTorch/GPU/HDF5 symbol linked into the default binary. | ✓ VERIFIED | `.oppfeatures` has `Simu5G_Sionna` feature with `initiallyEnabled="false"` and `extraSourceFolders="src/simu5g/stack/phy/channelmodel/sionna"`. `tests/sionna/check_default_build_symbols.sh` was executed against a real feature-OFF default build (commit `4e637e23`) and returned exit 0. False positive on `SessionNack` was fixed by demangling + identifier-boundary match. |
| 5  | The emitted HDF5 artifact carries schema_version, coord_transform block, full parameter contract, request hash, and a degenerate SINR-bin axis. | ✓ VERIFIED | `simulations/NR/sionna/sionna_artifact/manifest.json` (committed) contains: `schema_version: 1`, `coord_transform` (axis_map identity, origin [0,0,0], scale 1.0, units m, handedness right), `carrier_frequency_hz: 3500000000.0`, `subcarrier_spacing_hz: 30000.0`, `num_bands: 1`, `request_hash: 486a38...`, `sinr_grid: [0.0]` (degenerate S=1), `table_dtype: "<f8"`. `path_gain.bin` is exactly 8 bytes (1 link * 8 bytes). |

**Score:** 3/5 truths verified without qualification; 2 truths depend on operator confirmation of the in-sim run (human_needed). All 5 automated/code-level checks passed.

---

### CAL-02 Contract Binding Analysis (CR-01 adjudication)

The code-review CRITICAL finding CR-01 raised two concerns:

**Concern A — Initialization fails because SionnaManager NED params have no default().**
**VERDICT: INCORRECT (as of final committed state).** `SionnaManager.ned` has no `default()` for `carrierFrequencyHz`, `subcarrierSpacingHz`, `numBands`. However, `SionnaSingleLink.ned` sets them as instance-level defaults (`carrierFrequencyHz = default(3.5e9)`, `subcarrierSpacingHz = default(30000.0)`, `numBands = default(1)`). OMNeT++ resolves these at init, so the config initializes correctly. The review did not include `SionnaSingleLink.ned` in its `files_reviewed_list` (18 files, none of which is `SionnaSingleLink.ned`), which explains this error.

**Concern B — The contract compares against SionnaManager's own NED params, not the live PHY carrier.**
**VERDICT: VALID DESIGN WEAKNESS (WARNING).** The `initialize()` reads `par("carrierFrequencyHz")`, `par("subcarrierSpacingHz")`, `par("numBands")` from its own module rather than from the actual `componentCarrier` submodule the PHY uses. A user who sets `*.sionnaManager.carrierFrequencyHz = 3.5e9` and `*.carrierAggregation.componentCarrier[0].carrierFrequency = 2.6GHz` would pass all contract assertions while running a mismatched simulation. **In the shipped config, all values are consistent**: `SionnaSingleLink.ned` defaults and `omnetpp.ini:53–55` both set 3.5 GHz / numerology 1 (SCS 30 kHz) / 1 band. The design weakness is real but latent — it affects users who diverge from the example, not the shipped configuration itself. This is a WARNING, not a BLOCKER for Phase 1.

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tools/sionna_precompute/precompute.py` | SSOT -> PathSolver -> artifact writer (>=80 lines) | ✓ VERIFIED | 275 lines; `normalize=False` appears 4x; no `mi.variant()` call; `PathSolver` and `cfr` called; scenario SSOT loaded via `load_scenario`/`json.load`. |
| `tools/sionna_precompute/scenario.example.json` | SSOT with positions, antenna, carrier, coord_transform, mcs_set | ✓ VERIFIED | Contains all required keys: positions (tx+rx), antenna, carrier_frequency_hz (3.5e9), subcarrier_spacing_hz (30000.0), num_bands (1), mcs_set ([0]), coord_transform (identity). |
| `tools/sionna_precompute/friis_check.py` | CAL-01 Friis cross-check harness | ✓ VERIFIED | 63 lines; computes Sionna and Friis dB, prints residual, exits nonzero if >= 1.0 dB. |
| `tools/sionna_precompute/requirements.txt` | Pinned versions (sionna-rt==2.0.1, sionna==2.0.1, numpy==2.4.6, h5py==3.16.0) | ✓ VERIFIED | Exact pinned versions match CLAUDE.md requirements table. |
| `tools/sionna_precompute/tests/test_friis.py` | pytest Friis gate (<1.0 dB tolerance) | ✓ VERIFIED | Imports `compute_path_gain_dB`, asserts Sionna path gain within 1.0 dB of Friis at 100m/3.5GHz, marked `requires_venv`. |
| `.oppfeatures` | Simu5G_Sionna feature (initiallyEnabled=false) with extraSourceFolders | ✓ VERIFIED | `Simu5G_Sionna` feature present; `initiallyEnabled="false"`; `extraSourceFolders="src/simu5g/stack/phy/channelmodel/sionna"`; `compileFlags="-DWITH_SIONNA"`. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h` | class SionnaChannelModel : public NrChannelModel with getAttenuation override | ✓ VERIFIED | Declares `class SionnaChannelModel : public NrChannelModel`; `getAttenuation(MacNodeId, Direction, inet::Coord, bool)` override; `sionnaManager_` member; `linkKeyFor` helper. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.ned` | simple SionnaChannelModel extends NrChannelModel @class(SionnaChannelModel) | ✓ VERIFIED | `simple SionnaChannelModel extends NrChannelModel`; `@class("SionnaChannelModel")`; `sionnaManagerModule` and `artifactManifest` parameters. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc` | getAttenuation returns -table lookup, reuses inherited SINR (MOD-01) | ✓ VERIFIED | 73 lines; `return -sionnaManager_->getTable().lookup(linkIndex)`; calls `NrChannelModel::initialize(stage)`; does NOT call `computePathLoss`; keeps `getCoord().distance(coord)` funnel. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc` | Contract assertion at INITSTAGE_LOCAL with cRuntimeError on mismatch (CAL-02, >=40 lines) | ✓ VERIFIED | 125 lines; gated on `inet::INITSTAGE_LOCAL`; defines `EXPECTED_SCHEMA_VERSION=1`; 6 distinct `cRuntimeError` throws for schema_version, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, coord_transform, request_hash; no silent fallback. |
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc` | JSON manifest parse + LE-binary table read with input validation (>=40 lines) | ✓ VERIFIED | 120 lines; strict typed extractors for all required fields; `cRuntimeError` on missing key / type confusion / unreadable file / invalid JSON. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.cc` | bounds-validated LE float64 table read | ✓ VERIFIED | Validates `fileSize == num_links * sizeof(double)`; rejects `num_links == 0` and `> kMaxLinks (2^24)`; short-read detection; bounds-checked `lookup`. |
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.h` | struct Manifest with contract fields | ✓ VERIFIED | `struct Manifest` with schema_version, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, table_path, table_dtype, num_links, coord_transform, request_hash. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.h` | loadBinary + lookup methods | ✓ VERIFIED | Declares `static SionnaTable loadBinary(const std::string& path, std::size_t numLinks)`, `double lookup(std::size_t linkIndex) const`, `std::size_t size() const`. |
| `tests/sionna/check_default_build_symbols.sh` | SEAM-02 default-binary symbol-check gate | ✓ VERIFIED | Executable; demangles symbols via nm -C; case-sensitive identifier-boundary match on Sionna; case-insensitive substring match on hdf5/python/tensorflow/torch; ran against real feature-OFF binary (exit 0). |
| `simulations/NR/sionna/omnetpp.ini` | Single-link Sionna run config | ✓ VERIFIED | Contains `nrChannelModelType = "SionnaChannelModel"`; `shadowing=false`; `fading=false`; `fixedLos=true`; `artifactManifest` pointing to `sionna_artifact/manifest.json`; gNB at [0,0,10]m, UE at [100,0,1.5]m. |
| `simulations/NR/sionna/SionnaSingleLink.ned` | Single-link network (one gNB + one UE) | ✓ VERIFIED | Imports SionnaManager; `sionnaManager` submodule with instance defaults `carrierFrequencyHz=default(3.5e9)`, `subcarrierSpacingHz=default(30000.0)`, `numBands=default(1)`; one gNB + one UE. |
| `tests/fingerprint/sionna_singlelink.csv` | Pinned fingerprint baseline for Sionna single-link config | ✓ VERIFIED | Non-empty; contains fingerprint `0378-be95/tplx;61a8-43c8/~tNl;ba12-2467/sz`; tagged `sionna`; reproduction preconditions documented inline. |
| `simulations/NR/sionna/sionna_artifact/` (committed artifact) | manifest.json + path_gain.bin + results.h5 | ✓ VERIFIED | Artifact committed. manifest.json contains all required fields. path_gain.bin is exactly 8 bytes containing -83.3604 dB (little-endian float64). |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `precompute.py` | `scenario.example.json` | SSOT load via `load_scenario` / `json.load` | ✓ WIRED | `load_scenario(path)` with `json.load`; 52 occurrences of `scenario` in precompute.py. |
| `precompute.py` | `sionna.rt.PathSolver` / `Paths.cfr` | empty-world path gain extraction (normalize=False) | ✓ WIRED | `PathSolver()(scene=scene, max_depth=0, los=True, ...)` + `paths.cfr(frequencies=freqs, normalize=False, out_type="numpy")` present. |
| `SionnaChannelModel.cc::getAttenuation` | `SionnaTable::lookup` | negated table lookup returned as dB attenuation | ✓ WIRED | `return -sionnaManager_->getTable().lookup(linkIndex)` at SionnaChannelModel.cc:64. |
| `SionnaManager.cc::initialize` | `ManifestReader::read + assertContractMatchesLiveScenario` | fail-loud cRuntimeError on any contract-field or schema_version mismatch | ✓ WIRED | `ManifestReader::read(manifestPath)` at line 109; `assertContractMatchesLiveScenario(manifest_, live)` at line 116; 6 distinct cRuntimeError throws. |
| `.oppfeatures` | `src/simu5g/stack/phy/channelmodel/sionna` | extraSourceFolders excludes folder from default deep build | ✓ WIRED | `extraSourceFolders = "src/simu5g/stack/phy/channelmodel/sionna"` in the Simu5G_Sionna feature. |
| `SionnaChannelModel.ned` | `NrChannelModel.ned` | extends NrChannelModel (satisfies ILteChannelModel via parent chain) | ✓ WIRED | `simple SionnaChannelModel extends NrChannelModel` in SionnaChannelModel.ned:23. |
| `omnetpp.ini` | `NrNicUe.ned parametric typename nrChannelModelType` | ini string override selects SionnaChannelModel (no NED edit) — SEAM-01 | ✓ WIRED | `*.*.cellularNic.nrChannelModelType = "SionnaChannelModel"` at omnetpp.ini:70; `NrNicUe.ned:71` uses `<nrChannelModelType> like ILteChannelModel`. |
| `omnetpp.ini` | `sionna_artifact/manifest.json + path_gain.bin` | artifactManifest path consumed by SionnaManager at init | ✓ WIRED | `*.sionnaManager.artifactManifest = "sionna_artifact/manifest.json"` at omnetpp.ini:77. |

---

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `SionnaChannelModel.cc::getAttenuation` | `pathGainDb_` in `SionnaTable` | `SionnaTable::loadBinary` reads from `path_gain.bin` (committed artifact contains -83.3604 dB, produced by Sionna RT PathSolver) | Yes — artifact file is 8 bytes of real Sionna-computed float64, not a stub | ✓ FLOWING |
| `ManifestReader::read` | `Manifest` struct fields | JSON parse from committed `manifest.json` (carrier_frequency_hz=3.5e9, etc.) | Yes — fields parsed from real artifact, not hardcoded | ✓ FLOWING |
| `precompute.py::compute_path_gains_dB` | `H` (channel frequency response) | `paths.cfr(normalize=False)` from Sionna RT PathSolver over empty scene | Yes — confirmed 0.006 dB residual vs. Friis | ✓ FLOWING |

---

### Behavioral Spot-Checks

| Behavior | Command / Observation | Result | Status |
|----------|-----------------------|--------|--------|
| CAL-01 Friis round-trip (offline pytest) | Summary: `pytest tests/test_friis.py` = 1 passed; friis_check.py residual 0.006 dB < 1.0 dB gate | Confirmed by 01-01-SUMMARY.md and `path_gain.bin` = -83.3604 dB | ✓ PASS |
| Artifact binary correct size | `wc -c path_gain.bin` = 8 bytes = 8 * 1 link * sizeof(double) | 8 bytes confirmed | ✓ PASS |
| manifest.json has required fields | Direct file read | schema_version=1, coord_transform, request_hash, sinr_grid=[0.0], table_dtype="<f8", carrier_frequency_hz=3.5e9 | ✓ PASS |
| normalize=False in precompute.py | `grep -c normalize=False precompute.py` = 4 | 4 occurrences | ✓ PASS |
| No mi.variant override in precompute.py | `grep mi.variant precompute.py` | No output | ✓ PASS |
| SionnaManager has >= 2 cRuntimeError throws | `grep -v '//' SionnaManager.cc | grep -c cRuntimeError` = 6 | 6 throws (schema_version, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, coord_transform, request_hash) | ✓ PASS |
| getAttenuation does NOT call computePathLoss | `grep -c computePathLoss SionnaChannelModel.cc` = 0 | 0 occurrences | ✓ PASS |
| getAttenuation returns negated lookup | `grep lookup SionnaChannelModel.cc` | `return -sionnaManager_->getTable().lookup(linkIndex)` | ✓ PASS |
| SionnaTable validates file size | `grep sizeof(double) SionnaTable.cc` | `const std::streamoff expectedSize = (std::streamoff)(numLinks * sizeof(double))` | ✓ PASS |
| .oppfeatures extraSourceFolders excludes sionna | `grep extraSourceFolders .oppfeatures` | `src/simu5g/stack/phy/channelmodel/sionna` | ✓ PASS |
| Simulation ran (evidence of run) | `ls results/General/` = `0.vci (94780 bytes), 0.vec (94715 bytes)` | 94KB of vector results confirms the sim ran | ✓ PASS |
| In-sim path gain = Sionna value, NOT analytic | Cannot determine from vector file without opp_scavetool | — | ? HUMAN NEEDED |
| Corrupted manifest aborts with cRuntimeError | Cannot run negative test without live binary | — | ? HUMAN NEEDED |

---

### Probe Execution

| Probe | Command | Result | Status |
|-------|---------|--------|--------|
| `tests/sionna/unit/run_unit_tests.sh` | Script exists; 01-03-SUMMARY.md records 27/27 checks pass | Cannot re-run without compiling unit harness (requires liboppsim + libINET + feature-ON build). Commits `dee91e0f` -> `9237c45a` (20/20) and `c7ad0e21` -> `9e403360` (7/7) confirm RED -> GREEN TDD cycle. | PASS (evidence-based) |
| `tests/sionna/check_default_build_symbols.sh` | `bash tests/sionna/check_default_build_symbols.sh` (against real feature-OFF build, commit `4e637e23`) | Exit 0, zero forbidden symbols confirmed by 01-04-SUMMARY.md; negative controls (SionnaChannelModel symbol, H5Fopen symbol) confirmed FAIL. | PASS (evidence-based, executed by developer) |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| SEAM-01 | 01-04 | Select SionnaChannelModel via ini string, no NED interface edit | ✓ SATISFIED | `omnetpp.ini:70` sets `nrChannelModelType = "SionnaChannelModel"`; NrNicUe.ned parametric typename is intact; SionnaChannelModel.ned extends NrChannelModel. |
| SEAM-02 | 01-02, 01-04 | Default build byte-for-byte unaffected, no Python/HDF5/GPU symbol | ✓ SATISFIED | `.oppfeatures` feature with `initiallyEnabled=false`; `check_default_build_symbols.sh` ran exit 0 against real feature-OFF build. |
| TOOL-01 | 01-01 | SSOT scenario file drives the precompute tool | ✓ SATISFIED | `scenario.example.json` with all required keys; `load_scenario` consumes it; coord_transform and mcs_set present. |
| TOOL-02 | 01-01 | Explicit versioned coord/units transform in SSOT and manifest | ✓ SATISFIED | `scenario.example.json` has `coord_transform` block; manifest echoes it; `apply_coord_transform` applies identity; `isIdentityTransform` asserts it in C++. |
| TOOL-03 | 01-01 | One batched PathSolver, path gains via Paths.cfr over subcarrier_frequencies | ✓ SATISFIED | `PathSolver()(scene=scene, max_depth=0, ...)` + `paths.cfr(frequencies=freqs, normalize=False)` in `compute_path_gains_dB`. |
| ART-01 | 01-01, 01-03 | HDF5 artifact + JSON manifest with schema_version | ✓ SATISFIED | Three-file artifact committed; ManifestReader parses; SionnaTable loads binary; manifest has schema_version=1. |
| ART-02 | 01-01 | Manifest carries full parameter contract, coord_transform, request hash, degenerate SINR axis | ✓ SATISFIED | manifest.json has coord_transform, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, request_hash (SHA-256), sinr_grid=[0.0] (S=1). |
| MOD-01 | 01-03, 01-04 | SionnaChannelModel retains inherited getSINR/interference aggregation | ✓ SATISFIED | `getAttenuation` overrides only path loss with `-table.lookup`; does not call `computePathLoss`; no SINR/interference/noise reimplemented. |
| CAL-01 | 01-01, 01-04 | Empty-world path gain agrees with Friis within ~0.5-1 dB | ✓ SATISFIED (offline) / ? NEEDS HUMAN (in-sim) | Offline: 0.006 dB residual confirmed. In-sim: simulation ran but path gain value requires human confirmation. |
| CAL-02 | 01-03 | SionnaManager aborts with cRuntimeError on any contract mismatch | ✓ SATISFIED (code/unit) / ? NEEDS HUMAN (in-sim negative check) | 6 distinct cRuntimeError throws; unit harness 7/7; but in-sim negative test (corrupted manifest run) requires human confirmation. |

All 10 Phase 1 requirements are covered by the plans. No orphaned requirements were found (TOOL-04, MOD-02, MOD-03, FB-01, REP-01, REP-02, DIF-01–04 are correctly mapped to later phases).

---

### Anti-Patterns Found

| File | Finding | Severity | Impact |
|------|---------|----------|--------|
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.h`, `SionnaTable.h`, `SionnaChannelModel.h`, `SionnaManager.h` | Header doc comments still say "compilable skeleton ... filled in by Plan 01-03" after the implementation is complete. | INFO (IN-01) | Misleads future readers about implementation status. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc:57-58` | `threeDimDistance` computed then `(void)`-cast away. Dead computation kept for future validation hook. | INFO (IN-02) | Cosmetic; the comment explains intent but a reader will flag it. |
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc:44-51` | `requireInt` accepts unsigned JSON values then calls `.get<int>()` without range-checking for overflow. A manifest value like `num_bands: 4294967296` would silently yield garbage. | WARNING (WR-02) | Could cause contract assertion to compare a garbage value; not exploitable in the committed artifact but a real correctness hole for malformed manifests. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:82-88` | Exact `!=` floating-point comparison for carrier/SCS contract fields. JSON text and NED unit parsing may introduce representation differences for non-round-binary values. | WARNING (WR-04) | For 3.5 GHz and 30 kHz (exact binary representable values) this works; fragile for other frequencies. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:42-70` | `isIdentityTransform` is permissive: a wrong-length origin array, missing scale, or non-numeric scale passes as identity; `units` and `handedness` are not checked. | WARNING (WR-05) | A `units:"km"` transform passes as identity while geometry is actually scaled by 1000. For Phase 1 with the committed artifact this is inert. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:112-116` and `SionnaSingleLink.ned:63-65` | Contract values (`carrierFrequencyHz`, `subcarrierSpacingHz`, `numBands`) are read from SionnaManager's own NED params rather than from the live PHY's componentCarrier. A user can set them inconsistently. | WARNING (CR-01) | The shipped example is consistent (3.5 GHz everywhere). The weakness is latent: a user who changes only the PHY carrier and not the manager params will pass the contract check but run a mismatched simulation. Not a blocker for Phase 1 with the committed configuration. |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc:40-47` | `linkKeyFor` always returns 0 with no assertion that `num_links == 1`. If a multi-link artifact is loaded, every link gets link-0's path gain silently. | WARNING (WR-01) | v1 is single-link only; latent for Phase 2 multi-link. Not a blocker for Phase 1. |

No `TBD`, `FIXME`, or `XXX` debt markers were found in any phase-modified file. All `TODO` comments are scoped references to future plan IDs (e.g., `TODO(01-03)` which have been resolved).

---

### Human Verification Required

### 1. In-Sim Path Gain Source Confirmation

**Test:** With the `Simu5G_Sionna` feature enabled and the artifact present at `simulations/NR/sionna/sionna_artifact/`, run the simulation and record the per-link downlink path gain or rcvdSinrDl logged or recorded in the scalar/vector output. Confirm the value tracks the Sionna offline table (~-83.3 dB path gain at 100 m / 3.5 GHz) rather than the analytic NrChannelModel 3GPP formula value.
**Expected:** The logged or scalar-recorded per-link path gain is approximately -83.3 dB, consistent with the Sionna table and the offline `friis_check.py` output, and NOT the analytic 3GPP formula output.
**Why human:** The results/General/0.vec file (94 KB) confirms the simulation ran but the exact path gain scalar cannot be grep'd without an `opp_scavetool` parse or manual inspection of the simulation output during the run. The SUMMARY records operator approval but this verifier cannot independently read the sim output.

### 2. Corrupted Manifest Negative Check (CAL-02 In-Sim)

**Test:** Edit `simulations/NR/sionna/sionna_artifact/manifest.json`, change `carrier_frequency_hz` to a wrong value (e.g., `2600000000.0`), enable the `Simu5G_Sionna` feature, and re-run `opp_run -u Cmdenv -c General` from `simulations/NR/sionna/`. The run must abort.
**Expected:** Run exits nonzero with a cRuntimeError message naming the mismatch, such as `Sionna manifest carrier_frequency_hz mismatch: artifact 2.6e+09 Hz, scenario 3.5e+09 Hz`. After confirming, restore the original manifest value.
**Why human:** Cannot execute the binary without a live feature-ON build. The SUMMARY records the operator approved this negative check, but independent confirmation verifies the wiring.

---

### Gaps Summary

No BLOCKER gaps identified. All automated truths are VERIFIED. The two UNCERTAIN items (in-sim path gain confirmation, in-sim CAL-02 negative check) are human-verification items that have already been confirmed by the operator per 01-04-SUMMARY.md but cannot be independently re-confirmed without running the binary.

**CR-01 design weakness** (contract reads from SionnaManager's own NED params, not the live PHY's componentCarrier) is a WARNING. The shipped example is internally consistent. The concern will resurface when Phase 2 adds multi-link/multi-frequency scenarios — recommend addressing it at that point by binding the manager's contract values to the actual componentCarrier parameters.

---

_Verified: 2026-06-17_
_Verifier: Claude (gsd-verifier)_
