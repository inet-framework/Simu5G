---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
plan: 01
subsystem: offline-sionna-precompute-tool
tags: [sionna, ray-tracing, friis, artifact, ssot, tdd, python]
dependency_graph:
  requires: []
  provides:
    - "tools/sionna_precompute/precompute.py (SSOT -> PathSolver -> path gain -> artifact writer)"
    - "tools/sionna_precompute/scenario.example.json (shared scenario / SSOT contract)"
    - "Artifact contract: results.h5 + manifest.json + path_gain.bin (consumed by C++ side, Plan 01-03/01-04)"
  affects:
    - "Plan 01-03/01-04 C++ ManifestReader/SionnaManager (consume manifest.json schema + path_gain.bin LE float64 table)"
tech_stack:
  added:
    - "sionna-rt==2.0.1 (offline venv only)"
    - "sionna==2.0.1 (offline venv only)"
    - "numpy==2.4.6, h5py==3.16.0 (offline venv only)"
    - "pytest==7.4.4 (test framework, installed into offline venv)"
  patterns:
    - "RESEARCH Pattern 4: empty-world load_scene() + PathSolver(max_depth=0) + Paths.cfr(normalize=False) -> 10*log10(mean(|H|^2))"
    - "TOOL-02 explicit coord_transform (identity) recorded in manifest; OMNeT++ Euclidean distance is the Friis reference"
    - "Bulk floats only in LE-binary + HDF5; JSON manifest carries scalars/small lists (T-01-03)"
key_files:
  created:
    - "tools/sionna_precompute/precompute.py"
    - "tools/sionna_precompute/scenario.example.json"
    - "tools/sionna_precompute/friis_check.py"
    - "tools/sionna_precompute/tests/test_friis.py"
    - "tools/sionna_precompute/requirements.txt"
    - "tools/sionna_precompute/pytest.ini"
    - "tools/sionna_precompute/.gitignore"
  modified: []
decisions:
  - "Installed pytest 7.4.4 into the offline venv (test infra missing); pinned in requirements.txt as >=7,<8 per plan."
  - "compute_path_gains_dB structured for an [L]-link table; v1 has a single link but the table/manifest are already L-shaped."
  - "request_hash includes pinned lib_versions so a library bump invalidates the cache (reproducibility)."
metrics:
  duration_min: 3
  completed: "2026-06-17"
  tasks: 3
  files: 7
---

# Phase 1 Plan 01: Offline Sionna Precompute Tool Summary

Built the producer half of the walking skeleton: a Python-only offline tool that reads the shared scenario (SSOT), runs a single empty-world `PathSolver` over the Tx/Rx link, extracts the absolute per-link path gain via `Paths.cfr(normalize=False)`, and emits the versioned artifact set (canonical HDF5 + JSON manifest + little-endian float64 binary table). The empty-world Sionna path gain agrees with the textbook Friis value to within **0.006 dB** at 100 m / 3.5 GHz, proving the coord/units transform (TOOL-02) and the dB link-budget convention are correct.

## What Was Built

| Task | Description | Commit |
|------|-------------|--------|
| 1 (RED) | Failing CAL-01 Friis round-trip test + pinned `requirements.txt` + `requires_venv` marker | `59ac77f7` |
| 2 (GREEN) | SSOT `scenario.example.json`, `precompute.py` (validated load, identity coord_transform, empty-world path gain), standalone `friis_check.py` | `bc263f7d` |
| 3 | Artifact emission (`results.h5` + `manifest.json` + `path_gain.bin`) + output `.gitignore` | `973fcc97` |

## Verification Evidence

- `pytest tests/test_friis.py` -> **1 passed** under the offline venv (CAL-01 GREEN).
- `friis_check.py scenario.example.json` -> Sionna **-83.3604 dB** vs Friis **-83.3544 dB**, residual **0.0060 dB** < 1.0 dB gate, exit 0.
- `precompute.py scenario.example.json --out /tmp/sionna_art` -> emits all three files; `manifest.json` has `schema_version==1`, `coord_transform`, `request_hash`, `table_dtype=="<f8"`, `len(sinr_grid)==1`; `path_gain.bin` is exactly `8 * num_links` bytes. **ARTIFACT-OK**.
- `grep normalize=False precompute.py` matches (4x); non-comment `mi.variant(` count is **0** (variant never overridden).
- No bulk float array appears in any `.json` (the path-gain table lives only in `path_gain.bin` + HDF5).

## Requirements Exercised

- **TOOL-01** SSOT scenario file driving the tool.
- **TOOL-02** explicit `coord_transform` block (identity, recorded in manifest; OMNeT++ Euclidean distance is the Friis reference).
- **TOOL-03** one `PathSolver` per link, path gain via `Paths.cfr` over `subcarrier_frequencies`.
- **ART-01** three-file artifact set (HDF5 canonical + JSON manifest + LE-binary table).
- **ART-02** manifest carries `schema_version`, `coord_transform`, full parameter contract, `request_hash`, and a degenerate `sinr_grid` (S=1) so v2 interference curves are a purely additive `[L,S]` extension.
- **CAL-01** Friis round-trip gate green via both pytest and the standalone harness.

## TDD Gate Compliance

RED (`test(01-01): ...` `59ac77f7`) -> GREEN (`feat(01-01): ...` `bc263f7d`) -> artifact (`feat(01-01): ...` `973fcc97`). The RED test ran (not skipped) under the venv and failed with `ModuleNotFoundError: No module named 'precompute'` before `precompute.py` existed; it passes after. No refactor commit needed (code was clean on first GREEN).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] pytest absent from the offline venv**
- **Found during:** Task 1 (running the RED test).
- **Issue:** The offline venv had `sionna.rt` but no `pytest`, so the RED test could not be collected — `No module named pytest`. This is a test-framework infrastructure gap, not the intended RED (import error on `precompute`).
- **Fix:** Installed the plan-pinned `pytest>=7,<8` (resolved to 7.4.4) into the offline venv with its declared `pip`. This is sanctioned test-infra setup for the first TDD task (not a slopsquat-risk package add — pytest is the standard, plan-pinned test runner).
- **Files modified:** none (venv-only install; version already pinned in `requirements.txt`).
- **Commit:** infra install, no repo change.

### Threat Model

All `mitigate` dispositions were implemented:
- **T-01-01** (tampered SSOT): `validate_scenario` checks required keys/types and raises `ValueError` before any RT call.
- **T-01-03** (bulk floats in JSON): path-gain table written only to `path_gain.bin` + HDF5; manifest holds scalars/small lists.
- **T-01-02 / T-01-SC** (`accept`): `request_hash` is documented as a cache/integrity key only (not tamper protection); no new packages installed beyond the pre-provisioned/plan-pinned pytest.

## Authentication Gates

None.

## Known Stubs

None — the path-gain value is computed live from Sionna RT, not stubbed. `materials` is intentionally empty (empty-world v1 scope, per plan).

## Threat Flags

None — the tool introduces no new network/auth surface; it reads an author-supplied SSOT (validated) and writes local files.

## Self-Check: PASSED

All 7 created files exist on disk and all 3 task commits (`59ac77f7`, `bc263f7d`, `973fcc97`) are present in git history.
