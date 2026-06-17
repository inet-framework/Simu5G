---
phase: 1
slug: thin-end-to-end-slice-bring-up-empty-world-validation
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-17
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest 7.x (offline Sionna tool) + OMNeT++ fingerprint/`opp_run` harness + `nm` symbol-check (C++ side) |
| **Config file** | offline pytest under `tools/sionna_precompute/tests/`; Sionna fingerprint baseline `tests/fingerprint/sionna_singlelink.csv`; symbol gate `tests/sionna/check_default_build_symbols.sh` |
| **Quick run command** | `/home/zoli/Projects/OMNET/Sionna/venv/bin/python -m pytest tools/sionna_precompute/tests -q` (offline) / `bash tests/sionna/check_default_build_symbols.sh` (default-build symbol check) |
| **Full suite command** | offline pytest + `friis_check.py` + feature-OFF default build & symbol gate + feature-ON single-link Sionna run + Sionna fingerprint compare |
| **Estimated runtime** | ~60–120 seconds (RT empty-world solve dominates) |

---

## Sampling Rate

- **After every task commit:** Run the quick run command relevant to the task (offline `pytest -q` / `friis_check.py` for tool tasks; `grep`-based skeleton checks for C++ skeleton tasks; the symbol gate for build-isolation tasks)
- **After every plan wave:** Run the full suite command
- **Before `/gsd-verify-work`:** Full suite must be green (Friis gate passes, manifest assertion fires on mismatch, default build clean, fingerprint pinned)
- **Max feedback latency:** ~120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01 T1 | 01-01 | 1 | CAL-01 | T-01-01 | input keys validated in load_scenario | unit (Python, RED) | `cd tools/sionna_precompute && venv/bin/python -m pytest tests/test_friis.py -q` (expect RED) | ✅ created here | ✅ |
| 01-01 T2 | 01-01 | 1 | TOOL-01/02/03, CAL-01 | T-01-01 | normalize=False, variant not overridden | unit (Python, GREEN) | `cd tools/sionna_precompute && venv/bin/python -m pytest tests/test_friis.py -q && venv/bin/python friis_check.py scenario.example.json` | ✅ created here | ✅ |
| 01-01 T3 | 01-01 | 1 | ART-01, ART-02 | T-01-03 | bulk floats only in binary/HDF5, not JSON | unit (Python) | `venv/bin/python precompute.py scenario.example.json --out /tmp/sionna_art && venv/bin/python -c "...assert schema_version==1, coord_transform, request_hash, sinr_grid len 1, table size==8*num_links..."` | ✅ created here | ✅ |
| 01-02 T1 | 01-02 | 1 | SEAM-02 | T-02-01 | feature excludes sionna/ from default build | unit (config + script syntax) | `grep -q Simu5G_Sionna .oppfeatures && grep -q 'extraSourceFolders.*channelmodel/sionna' .oppfeatures && bash -n tests/sionna/check_default_build_symbols.sh` | ✅ created here | ✅ |
| 01-02 T2a | 01-02 | 1 | SEAM-01 (registration) | T-02-01 | no external deps in skeleton | unit (grep) | `grep -q 'class SionnaChannelModel : public NrChannelModel' ... && grep -q 'Define_Module(SionnaChannelModel)' ... && grep -q 'Define_Module(SionnaManager)' ...` → SKELETON-A-OK | ✅ created here | ✅ |
| 01-02 T2b | 01-02 | 1 | SEAM-02 (no deps) | T-02-02 | no Sionna/HDF5/Python headers; no Define_Module | unit (grep) | `... struct Manifest, loadBinary present; ! Define_Module in SionnaTable.cc/ManifestReader.cc` → SKELETON-B-OK | ✅ created here | ✅ |
| 01-03 T1 | 01-03 | 2 | ART-01, V5 | T-03-01/02 | bounds-checked binary read, typed JSON parse | unit (grep + behavior) | `grep -q cRuntimeError ManifestReader.cc && grep -q cRuntimeError SionnaTable.cc && grep -q 'sizeof(double)' SionnaTable.cc && grep -q schema_version ManifestReader.cc` → READER-OK | ✅ created here | ✅ |
| 01-03 T2 | 01-03 | 2 | CAL-02 | T-03-03 | assert every contract field, no silent fallback | unit (grep + behavior) | `grep -q INITSTAGE_LOCAL SionnaManager.cc && grep -q EXPECTED_SCHEMA_VERSION ... && cRuntimeError count >= 2` → MANAGER-OK | ✅ created here | ✅ |
| 01-03 T3 | 01-03 | 2 | MOD-01 | — | inherited SINR reused; no computePathLoss | unit (grep) | `grep -q 'getCoord().distance(coord)' ... && grep -q lookup ... && grep -q 'NrChannelModel::initialize' ... && ! grep -q computePathLoss ...` → OVERRIDE-OK | ✅ created here | ✅ |
| 01-04 T1 | 01-04 | 3 | SEAM-01, MOD-01 | T-04-01 | no NED interface edit | smoke (grep) | `grep -q 'nrChannelModelType.*"SionnaChannelModel"' omnetpp.ini && shadowing/fixedLos/artifactManifest present && SionnaSingleLink.ned exists` → CONFIG-OK | ✅ created here | ✅ |
| 01-04 T2 | 01-04 | 3 | CAL-01, MOD-01 (+CAL-02 negative) | T-04-01 | corrupted manifest aborts | integration (human-verify) | human-check; automated companions: `friis_check.py` (01-01 T2), SEAM-02 gate (01-04 T4), fingerprint (01-04 T3) | ✅ run config created here | ✅ (human) |
| 01-04 T3 | 01-04 | 3 | REP-01 groundwork | T-04-03 | pinned deterministic baseline | fingerprint | `test -s tests/fingerprint/sionna_singlelink.csv && grep -qi sionna tests/fingerprint/sionna_singlelink.csv` → FINGERPRINT-OK | ✅ created here | ✅ |
| 01-04 T4 | 01-04 | 3 | SEAM-02 | T-04-03 | feature-OFF build links zero sionna/hdf5/python symbols | unit (real build + symbol grep) | `bash -n tests/sionna/check_default_build_symbols.sh && bash tests/sionna/check_default_build_symbols.sh` → SEAM02-OK | ✅ created here | ✅ |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky · (Status column reflects "has automated/companion coverage planned", not a live run result.)*

---

## Wave 0 Requirements

> Wave 0 scaffolding is folded into Wave 1 tasks (no separate Wave 0 plan): the offline test scaffold, the symbol-check script, and the run config are all created by Wave 1/3 tasks that begin with a failing/grep-able verify. All Wave 0 gaps are therefore covered.

- [x] Offline tool test scaffold (`tools/sionna_precompute/tests/test_friis.py`) — Friis round-trip assertion harness for CAL-01 (created by 01-01 Task 1 as the RED step)
- [x] `nm` default-build symbol-check script (`tests/sionna/check_default_build_symbols.sh`) — proves no Sionna/HDF5/Python symbols in default binary, SEAM-02 (authored by 01-02 Task 1; executed for real against a feature-OFF build by 01-04 Task 4)
- [x] Pinned single-link Sionna fingerprint baseline under `tests/fingerprint/sionna_singlelink.csv` — created by 01-04 Task 3 (ART-01/MOD-01 reproducibility)
- [x] Default-build byte-for-byte reference — the automated SEAM-02 symbol gate (01-04 Task 4) on a feature-OFF build is the byte-clean proof; the existing `tests/fingerprint/` baselines cover unchanged-run regression

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| End-to-end single-link run completes with SionnaChannelModel active and in-sim path gain tracks the Friis/Sionna value | CAL-01, MOD-01 | Requires a feature-ON build + interactive confirmation that the recorded rcvdSinrDl/path-gain reflects the Sionna table (not the analytic formula) | 01-04 Task 2 how-to-verify steps 1–5; automated companions exist (offline `friis_check.py`, the SEAM-02 gate, and the pinned fingerprint), so the manual step is confirmation, not the sole proof |

*Target met: every requirement has at least one automated verify; the single human-verify (01-04 T2) is backed by automated companion gates (CAL-01 via `friis_check.py`, CAL-02 logic via 01-03 T2 greps, SEAM-02 via 01-04 T4, determinism via 01-04 T3 fingerprint).*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (the one human-verify task has automated companion coverage)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify (every task except 01-04 T2 has `<automated>`; T2 is bracketed by automated T1/T3/T4)
- [x] Wave 0 covers all MISSING references (folded into Wave 1/3 tasks)
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved (planner, revision pass 2026-06-17)
