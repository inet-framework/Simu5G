---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
plan: 04
subsystem: testing
tags: [omnetpp, ned, ini, channelmodel, sionna, fingerprint, oppfeatures, seam, friis, empty-world]

# Dependency graph
requires:
  - phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation (Plan 01-01)
    provides: offline Sionna artifact (manifest.json + path_gain.bin) + Friis-vs-Sionna empty-world validation
  - phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation (Plan 01-02)
    provides: Simu5G_Sionna build-isolation feature (initiallyEnabled=false) + SEAM-02 symbol-check gate script
  - phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation (Plan 01-03)
    provides: loaded C++ consumer (getAttenuation table lookup, ManifestReader/SionnaTable, CAL-02 contract assertion)
provides:
  - End-to-end single-link empty-world slice: ini selects SionnaChannelModel via the nrChannelModelType parametric typename only (SEAM-01), runs on the Sionna per-link path gain (MOD-01), validated by the in-sim Friis round-trip (CAL-01)
  - Pinned Sionna single-link fingerprint baseline (tests/fingerprint/sionna_singlelink.csv) — deterministic regression anchor (REP-01 groundwork)
  - Genuinely-executed SEAM-02 default-build symbol gate proving the feature-OFF binary links zero Sionna/HDF5/Python/TensorFlow/Torch symbols
affects: [Phase 2 (multi-link slice extends this single-link config), Phase 3 (REP-01/REP-02 reproducibility hardening builds on this baseline)]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Fingerprint baseline capture: seed a one-row CSV with canonical ingredients (tplx;~tNl;sz) + placeholder hashes, run fingerprinttest.py against the feature-ON build, read the .UPDATED file for the computed hashes"
    - "SEAM-02 gate matches DEMANGLED symbols in two precise passes: identifier-boundary case-sensitive `Sionna` + case-insensitive substring hdf5|python|tensorflow|torch"

key-files:
  created:
    - tests/fingerprint/sionna_singlelink.csv
  modified:
    - tests/sionna/check_default_build_symbols.sh

key-decisions:
  - "Captured the fingerprint via the project's own fingerprinttest.py harness (not a hand-rolled opp_run invocation) so the recorded ingredient set and format match the existing simulations.csv baselines exactly; harness handles the ~tNl modifier that the bare opp_run command line rejects"
  - "Recorded the baseline against the feature-ON DEBUG build (libsimu5g_dbg.so, the build that exists this session); fingerprint ingredients (tplx/~tNl/sz) are build-config-independent, confirmed deterministic across two reruns"
  - "Left .oppfeatures committed as initiallyEnabled=false (default-OFF, unchanged); .oppfeaturestate is gitignored local build-state, so disabling the feature for Task 4 produced no tracked change"
  - "Fixed the SEAM-02 gate's symbol-matching to demangle and anchor `Sionna` at an identifier boundary, eliminating a false positive on RtVideoStreamingSender::handleStartSessionNack without weakening the forbidden-symbol set (verified by negative controls)"

patterns-established:
  - "Sionna config baselines are the config's OWN, never matched to the analytic-model baselines (REP-01 intent) — documented inline with reproduction preconditions"
  - "Build-isolation gates must be run against a real binary, not just bash -n: doing so this session surfaced two latent gate bugs that syntax-checking could never catch"

requirements-completed: [SEAM-01, MOD-01, CAL-01, SEAM-02]

# Metrics
duration: ~35min (continuation session, Tasks 3-4)
completed: 2026-06-17
---

# Phase 1 Plan 04: End-to-End Empty-World Slice + Fingerprint Baseline + SEAM-02 Gate Summary

**The walking-skeleton loop closes end-to-end: an ini-only SionnaChannelModel selection runs a single-link empty-world simulation on the Sionna path gain (Friis round-trip ~70.14 dB closes in-sim), pinned by a deterministic fingerprint baseline, with the feature-OFF default build provably free of Sionna/HDF5/Python symbols.**

## Performance

- **Duration:** ~35 min (continuation session covering Tasks 3-4; Tasks 1-2 completed in a prior session)
- **Started (this session):** 2026-06-17 (resume after human-verify approval)
- **Completed:** 2026-06-17
- **Tasks:** 4 (Tasks 1-2 prior + verified; Tasks 3-4 executed this session)
- **Files modified (this session):** 2 (1 created fingerprint baseline + 1 modified gate script)

## Accomplishments
- **Task 3 — Sionna single-link fingerprint baseline pinned.** Ran the Sionna empty-world config (`-c General`, 1s sim time) through the project's `fingerprinttest.py` harness against the feature-ON build and the committed artifact; recorded `0378-be95/tplx;61a8-43c8/~tNl;ba12-2467/sz` into `tests/fingerprint/sionna_singlelink.csv` in the canonical column format, with a header documenting feature-ON + artifact-present reproduction preconditions. Deterministic across two reruns.
- **Task 4 — SEAM-02 default-build symbol gate executed for real.** Disabled `Simu5G_Sionna`, regenerated makefiles (so the sionna/ source folder is excluded via `-Xsimu5g/stack/phy/channelmodel/sionna`), rebuilt the default debug + release libraries clean, and ran `tests/sionna/check_default_build_symbols.sh` — exit 0, zero forbidden symbols. This is the genuine automated SEAM-02 proof that Plan 01-02 could only `bash -n` syntax-check.
- **Verified (not redone):** Tasks 1-2 commits `4ba5e62d` (single-link network + ini SEAM-01 selection) and `af9adcf2` (pinned artifact + ini IPv4 configurator fix) present in history; the human-verify end-to-end run + CAL-02 negative check were operator-approved.

## Task Commits

Each task was committed atomically:

1. **Task 1: Single-link NR network + ini selecting SionnaChannelModel via parametric typename (SEAM-01, MOD-01)** - `4ba5e62d` (feat) — *prior session, verified*
2. **Task 1 env-prep: Pin artifact + fix ini IPv4 configurator** - `af9adcf2` (feat) — *prior session, verified*
3. **Task 2: End-to-end run + CAL-01 round-trip + CAL-02 negative check** - operator-approved via human-verify (no separate code commit beyond the above)
4. **Task 3: Pin Sionna single-link fingerprint baseline (REP-01 groundwork)** - `93560dea` (test) — *this session*
5. **Task 4: Execute SEAM-02 symbol gate on real feature-OFF build + fix gate** - `4e637e23` (test) — *this session*

**Plan metadata:** (this commit) `docs(01-04): complete plan`

## Files Created/Modified
- `tests/fingerprint/sionna_singlelink.csv` (created) - The Sionna single-link config's own fingerprint baseline (`0378-be95/tplx;61a8-43c8/~tNl;ba12-2467/sz`), with reproduction preconditions documented inline.
- `tests/sionna/check_default_build_symbols.sh` (modified) - SEAM-02 gate: fixed binary resolution (probe `libsimu5g*.so` first; skip the `bin/simu5g` launcher *script* that `nm` cannot read; pipefail-safe readability helper) and match precision (demangle via `nm -C`; anchor `Sionna` at an identifier boundary, case-sensitive; keep hdf5/python/tensorflow/torch as case-insensitive substrings).

## Decisions Made
- **Harness-driven fingerprint capture.** A bare `opp_run --fingerprint=...` command line rejects the `~tNl` ingredient modifier (`Unknown fingerprint ingredient character '~'`) and the `;` group separator in this OMNeT++ 6.4 build. The project's `fingerprinttest.py` passes these correctly via subprocess and emits a `.UPDATED` row in the exact committed format — so the baseline was captured through the harness, guaranteeing format parity with `simulations.csv`.
- **Debug build is the recording reference.** The only feature-ON library that exists this session is the debug build (`libsimu5g_dbg.so`, built 21:24); the committed release `libsimu5g.so` is a stale 2024 artifact (ABI-incompatible with current INET). The `tplx/~tNl/sz` fingerprint ingredients are topology/event-count/scalar-result hashes that do not depend on debug-vs-release, and the value was identical on two reruns.
- **Feature left in committed default-OFF state.** `.oppfeatures` keeps `initiallyEnabled="false"` (unchanged); the local `.oppfeaturestate` toggled to disabled is gitignored. The repo therefore ships the default-OFF feature state with no tracked diff, satisfying the "default build byte-for-byte unaffected" constraint.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] SEAM-02 gate false-positived on `SessionNack` and dead-ended on the launcher script**
- **Found during:** Task 4 (executing the gate against a real feature-OFF build for the first time)
- **Issue:** Two latent bugs in `check_default_build_symbols.sh` that `bash -n` (the only check Plan 01-02 could perform) cannot catch:
  (a) **Match precision** — the case-insensitive substring pattern `sionna` matched `simu5g::RtVideoStreamingSender::handleStartSessio`**`nNa`**`ck` (the substring "ssionNa"), reporting a false SEAM-02 violation on a feature-OFF build that contains zero actual Sionna code.
  (b) **Binary resolution** — `bin/simu5g` is an `opp_run` launcher *shell script* on Linux, not an ELF object; `nm` yields no output, so default resolution dead-ended with exit 2 ("could not read any symbols") instead of inspecting the real `libsimu5g*.so` shared libraries.
- **Fix:** (a) Demangle symbols (`nm -C` / `c++filt` fallback) and match `Sionna` at an identifier boundary, case-sensitive, in a dedicated pass; keep hdf5/python/tensorflow/torch as case-insensitive substrings. (b) Probe the `libsimu5g*.so` shared libraries first; add a pipefail-safe `nm_readable()` helper that skips candidates `nm` cannot read (so the launcher script is bypassed rather than masking a readable library). The forbidden-symbol *set* is unchanged — only precision and resolution were fixed (explicitly authorized by the Task 4 action: "fix only its binary-resolution logic ... do not weaken the grep set").
- **Files modified:** `tests/sionna/check_default_build_symbols.sh`
- **Verification:** PASS (exit 0) on the fresh feature-OFF `libsimu5g_dbg.so` and `libsimu5g.so`; FAIL (exit 1) on two negative controls — a `simu5g::SionnaChannelModel::getAttenuation()` symbol and an `H5Fopen_hdf5_stub` symbol — confirming detection of both real Sionna code and dependency-library symbols is intact.
- **Committed in:** `4e637e23` (Task 4 commit)

---

**Total deviations:** 1 auto-fixed (1 bug, scoped strictly to the gate's resolution + matching precision).
**Impact on plan:** Necessary for the gate to function as a genuine automated proof rather than a syntax-only check — which is precisely Task 4's stated purpose. No scope creep; forbidden-symbol coverage unweakened.

## Issues Encountered
- **`opp_run` fingerprint argument parsing.** The bare `--fingerprint=` command line in this OMNeT++ 6.4 build rejects both the `~` ingredient modifier and the `;` group separator used in the on-disk CSV format. Resolved by capturing through `fingerprinttest.py`, which is the canonical path for recording baselines anyway.
- **Stale committed release library.** `src/libsimu5g.so` (Nov 2024) is ABI-incompatible with the current INET (`undefined symbol: inet::MobilityBase::handleParameterChange`), so it cannot run simulations. Not in scope to fix here — the Task 4 build produced fresh feature-OFF debug + release libraries; the fingerprint was captured with the working debug build. (Logged as an environment note, not a code defect in this plan.)

## User Setup Required
None - no external service configuration required. (Running the Sionna config requires enabling the `Simu5G_Sionna` feature and generating the offline artifact, both documented in `simulations/NR/sionna/omnetpp.ini` and the fingerprint baseline header; these are opt-in research steps, not a default-build requirement.)

## Known Stubs
None introduced by this plan. (The Phase-1-scope scalar-per-link table and EESM defaults are intentional v1 design per PROJECT.md, not stubs.)

## Next Phase Readiness
- The end-to-end empty-world slice is closed and validated (SEAM-01 + MOD-01 + CAL-01 exercised end-to-end; SEAM-02 proven by an executed gate). Phase 1 walking skeleton is complete.
- A deterministic fingerprint anchor exists for the Sionna config, ready for REP-01/REP-02 reproducibility hardening in Phase 3.
- The SEAM-02 gate is now a real, reusable CI-able check; future phases can add it to the fingerprint/CI workflow.
- Concern (non-blocking): the committed release `libsimu5g.so` should be rebuilt against current INET in a future build/CI refresh so `bin/simu5g` (release) is runnable; this plan's work used and rebuilt the debug + release libs locally.

## Self-Check: PASSED

- `tests/fingerprint/sionna_singlelink.csv` exists and is non-empty (FINGERPRINT-OK).
- `tests/sionna/check_default_build_symbols.sh` runs against the real feature-OFF build with exit 0 (SEAM02-OK), and FAILs on negative controls.
- Commits `93560dea` (Task 3) and `4e637e23` (Task 4) present in git history; prior commits `4ba5e62d` and `af9adcf2` verified present.

---
*Phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation*
*Completed: 2026-06-17*
