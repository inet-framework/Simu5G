---
phase: 2
slug: full-bler-table-cqi-feedback-consistency
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-18
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from 02-RESEARCH.md §Validation Architecture (verified against the live venv + source tree).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework (offline)** | pytest 7.4.4 in the Sionna venv; marker `requires_venv` |
| **Framework (C++)** | standalone harness `tests/sionna/unit/run_unit_tests.sh` (liboppsim/libINET, no kernel) |
| **Config file** | `tools/sionna_precompute/pytest.ini` |
| **Quick run command** | `/home/zoli/Projects/OMNET/Sionna/venv/bin/python -m pytest tools/sionna_precompute/tests -q` and/or `tests/sionna/unit/run_unit_tests.sh` |
| **Full suite command** | full pytest (`requires_venv`) + full C++ harness + feature-ON build + Sionna single-link run + fingerprint re-pin |
| **Estimated runtime** | offline pytest ~a few s; C++ harness ~tens of s; phase-gate sim run ~minutes |

---

## Sampling Rate

- **After every task commit:** Run the relevant quick command (pytest subset or C++ harness).
- **After every plan wave:** Run full pytest (`requires_venv`) + full C++ harness.
- **Before `/gsd-verify-work`:** feature-ON build green, Sionna config runs, fingerprint re-pinned, full suites green.
- **Max feedback latency:** < 120 s for quick commands.

---

## Per-Task Verification Map

> Mapped at requirement level; the planner aligns concrete task IDs (`02-PP-TT`) to these rows.

| Requirement | Wave | Behavior | Test Type | Automated Command | File Exists | Status |
|-------------|------|----------|-----------|-------------------|-------------|--------|
| TOOL-04 | 0/1 | BLER[L,M] monotone in MCS, deterministic, finite for MCS 2–27 (DL `mcs_category=1`) | unit (pytest) | `python -m pytest tools/sionna_precompute/tests/test_bler_table.py -x` | ❌ W0 | ⬜ pending |
| TOOL-04 | 0/1 | `inf` sentinel rejected for MCS 0/1; tool raises | unit (pytest) | same file | ❌ W0 | ⬜ pending |
| ART-02 | 0/1 | manifest carries `mcs_table_index`, `mcs_category`, `num_mcs`, `sinr_effective_fun`; `bler.bin` size == `L*num_mcs*8` | unit (pytest + C++) | pytest artifact test + `test_bler_lookup.cc` | ❌ W0 | ⬜ pending |
| MOD-03 / D-05/D-06 | 0/2 | `lookupBler` bounds-checks (bad link/mcs/table-index → `cRuntimeError`) | unit (C++) | `tests/sionna/unit/run_unit_tests.sh` | ❌ W0 (extend) | ⬜ pending |
| D-02 | 0/2 | shared `cqiToMcsIndex`/inverse round-trips; identical in both readers | unit (C++) | `test_bler_lookup.cc` | ❌ W0 | ⬜ pending |
| FB-01 / D-08 | 2/3 | self-consistency: feedback-selected MCS BLER ≤ `targetBler_` per link (hard `cRuntimeError`) | unit (C++) + sim | `test_bler_lookup.cc` (pure check) + feature-ON run | ❌ W0 | ⬜ pending |
| MOD-03 | 3 | reception uses Sionna BLER + keeps `uniform(0,1)≤BLER` draw / `harqReduction_`; fingerprint stable & re-pinned | integration (fingerprint) | `fingerprinttest.py` on the Sionna config | ⚠️ re-pin existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tools/sionna_precompute/tests/test_bler_table.py` — TOOL-04 (monotonicity, determinism, `inf`-rejection, DL `mcs_category=1`, MCS 2–27 range).
- [ ] `tests/sionna/unit/test_bler_lookup.cc` — D-05/D-06 lookup bounds, D-02 CQI↔MCS bridge round-trip, D-08 self-check pure logic.
- [ ] No new framework install (pytest + C++ harness already exist).

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Controlled-link test: scheduler MCS and realized BLER move together (no chronic BLER ≫ target) | Phase 2 success criterion 4 | Needs a live feature-ON sim run + scalar inspection (`opp_scavetool`) | Run the Sionna single-link config feature-ON; confirm the feedback-selected MCS's realized BLER tracks `targetBler_`; observe via 0.sca |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (`test_bler_table.py`, `test_bler_lookup.cc`)
- [ ] No watch-mode flags
- [ ] Feedback latency < 120s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
