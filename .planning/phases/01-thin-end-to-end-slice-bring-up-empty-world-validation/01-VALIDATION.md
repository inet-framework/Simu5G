---
phase: 1
slug: thin-end-to-end-slice-bring-up-empty-world-validation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-17
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest 7.x (offline Sionna tool) + OMNeT++ fingerprint/`opp_run` harness (C++ side) |
| **Config file** | none — Wave 0 installs (offline `tests/`); fingerprint baseline under `tests/fingerprint/` |
| **Quick run command** | `pytest -q Sionna/tests` (offline) / `nm -D bin/simu5g \| grep -iE 'sionna\|hdf5\|python'` (default-build symbol check) |
| **Full suite command** | offline `pytest Sionna/tests` + Sionna single-link fingerprint run + default-build byte-for-byte baseline diff |
| **Estimated runtime** | ~60–120 seconds (RT empty-world solve dominates) |

---

## Sampling Rate

- **After every task commit:** Run the quick run command relevant to the task (offline `pytest -q`, or the `nm` symbol check for build-isolation tasks)
- **After every plan wave:** Run the full suite command
- **Before `/gsd-verify-work`:** Full suite must be green (Friis gate passes, manifest assertion fires on mismatch, default build clean)
- **Max feedback latency:** ~120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD — populated by planner | — | — | SEAM-01/02, TOOL-01/02/03, ART-01/02, MOD-01, CAL-01/02 | — | N/A | unit / integration | TBD | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Offline tool test scaffold (`Sionna/tests/`) — Friis round-trip assertion harness for CAL-01/CAL-02
- [ ] `nm`/`ldd` default-build symbol-check script — proves no Sionna/HDF5/Python symbols in default binary (SEAM-02)
- [ ] Pinned single-link Sionna fingerprint baseline under `tests/fingerprint/` (ART-01, MOD-01 reproducibility)
- [ ] Default-build byte-for-byte baseline capture (pre-integration reference output)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| TBD — planner to confirm whether any behavior lacks automated coverage | — | — | — |

*Target: all phase behaviors have automated verification (Friis gate, manifest fail-loud, symbol check, baseline diff are all scriptable).*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 120s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
