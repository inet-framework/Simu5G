# Roadmap: Simu5G × Sionna RT Integration

## Overview

This roadmap delivers an optional, opt-in `SionnaChannelModel` for Simu5G as a sequence of
**end-to-end vertical slices** (Vertical MVP), not isolated technical layers. The first phase is a
thin, complete bring-up-and-validation slice: a minimal shared scenario (SSOT) drives a minimal
offline Sionna tool over an empty world with a single link, producing a minimal HDF5 artifact +
manifest, which a minimal `SionnaChannelModel` loads and runs a single-link simulation against —
proven correct by the empty-world Friis round-trip check. Each later phase widens the same working
slice: the full per-(link,MCS) BLER table with CQI/feedback consistency; then correctness hardening
(no double-counting, two-seed determinism, request-hash cache, pinned fingerprint baselines, and a
byte-for-byte unchanged default build); then the differentiators that motivate opting in (authored
real-map Munich scene, path-gain RSRP, optional auto-invocation, calibration report). The artifact's
`schema_version` + `coord_transform` contract and the fail-loud parameter assertion are foundational
and ship in minimal form inside Phase 1 because every later slice extends them; the v1 noise-limited
table is designed as a strict subset of v2 (degenerate SINR-bin axis) from day one. v2 items
(interference curves, mobility, per-RB MIMO, INET-scene derivation) are explicitly out of this roadmap.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Thin End-to-End Slice (Bring-Up & Empty-World Validation)** - One SSOT → one empty-world link → one HDF5 artifact → one loaded `SionnaChannelModel` run, validated by the Friis round-trip; default build provably Python/HDF5-free.
- [ ] **Phase 2: Full BLER Table & CQI/Feedback Consistency** - Widen to per-(link,MCS) BLER for all CQIs/MCS; one table drives both reception and CQI feedback, with a self-consistency gate.
- [ ] **Phase 3: Correctness Hardening (No-Double-Counting, Determinism, Cache, Fingerprints)** - Suppress statistical path-loss terms, prove two-seed bit-identical RSRP, add request-hash cache, pin Sionna fingerprint baselines, and prove the default build is byte-for-byte unchanged.
- [ ] **Phase 4: Differentiators (Authored Scene, Path-Gain RSRP, Auto-Invoke, Calibration Report)** - Additive site-specific value on the calibrated core: Munich real-map scene, Sionna-derived RSRP, opt-in auto-invocation, and a bounded-difference calibration report.

## Phase Details

### Phase 1: Thin End-to-End Slice (Bring-Up & Empty-World Validation)

**Goal**: A researcher can run one complete Sionna pipeline — author a minimal shared scenario, run the offline tool over an empty world with a single Tx/Rx link, produce a versioned HDF5 artifact + JSON manifest, select `SionnaChannelModel` via ini, and run a single-link simulation whose path gain comes from Sionna — and trust it because the empty-world Friis round-trip and the fail-loud manifest assertion both pass; meanwhile a default build/run is provably free of Python/HDF5.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: SEAM-01, SEAM-02, TOOL-01, TOOL-02, TOOL-03, ART-01, ART-02, MOD-01, CAL-01, CAL-02
**Success Criteria** (what must be TRUE):

  1. A researcher selects `SionnaChannelModel` purely via the `lteChannelModelType`/`nrChannelModelType` ini string (no NED interface edit), and the single-link empty-world simulation loads the artifact and runs end-to-end using the Sionna per-link path gain.
  2. The empty-world Friis round-trip check passes: Sionna's LOS path gain for the link agrees with the textbook free-space (Friis) value at the same OMNeT++ Euclidean distance within the physics-derived tolerance (~0.5–1 dB), confirming the `coord_transform` and dB/linear link-budget convention are correct.
  3. `SionnaManager` asserts the artifact manifest against the live scenario at init and aborts with `cRuntimeError` on any contract-field or `schema_version` mismatch (no silent fallback).
  4. A default `make` build and a normal simulation run produce output byte-for-byte identical to the pre-integration baseline, with no Python / TensorFlow / PyTorch / GPU / HDF5 symbol linked into the default binary.
  5. The emitted HDF5 artifact carries `schema_version`, the `coord_transform` block, the full parameter contract, the request hash, and a present-but-degenerate SINR-bin axis, so v2 interference curves are a purely additive extension.

**Plans**: 4 plansPlans:
**Wave 1**

- [x] 01-01-PLAN.md — Offline Sionna precompute tool: SSOT → empty-world PathSolver → path gain → artifact (HDF5 + manifest + LE-binary), Friis round-trip gate (TOOL-01/02/03, ART-01/02, CAL-01)
- [ ] 01-02-PLAN.md — Build isolation: Simu5G_Sionna .oppfeatures feature + compilable C++/NED skeleton + default-binary symbol-check gate (SEAM-02)

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 01-03-PLAN.md — C++ consumer: ManifestReader/SionnaTable load+validate, SionnaManager fail-loud contract assertion, SionnaChannelModel::getAttenuation (MOD-01, CAL-02, ART-01)

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 01-04-PLAN.md — End-to-end run: single-link network + ini selection, Friis round-trip closes in-sim, pinned fingerprint baseline (SEAM-01, MOD-01, CAL-01)

### Phase 2: Full BLER Table & CQI/Feedback Consistency

**Goal**: The offline tool produces BLER for every CQI/MCS per link via `sionna.sys.PHYAbstraction`, and both the reception path and the CQI-feedback path read that one `SionnaTable`, so the scheduler's chosen MCS and the realized BLER provably agree.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: TOOL-04, MOD-03, FB-01
**Success Criteria** (what must be TRUE):

  1. The artifact holds a BLER value for all CQIs/MCS per link (not just the operating MCS), computed via `sionna.sys.PHYAbstraction` (effective-SINR → shipped 5G-NR LDPC reference tables, EESM) with a pinned `mcs_table_index`.
  2. Reception decisions use the `SionnaTable` BLER lookup while retaining the `uniform(0,1) ≤ BLER` success draw and the `harqReduction_` HARQ heuristic on top.
  3. `SionnaFeedbackComputation::getCqi()` selects the highest MCS with BLER ≤ `targetBler_` from the **same** `SionnaTable`, and an init self-consistency check confirms each link's feedback-selected MCS has realized BLER ≤ `targetBler_` on that table.
  4. A controlled-link test shows scheduler MCS and realized BLER move together (no chronic BLER far above target from table disagreement).

**Plans**: TBD

### Phase 3: Correctness Hardening (No-Double-Counting, Determinism, Cache, Fingerprints)

**Goal**: With `SionnaChannelModel` active, Simu5G's statistical path-loss terms are fully suppressed (Sionna owns path gain), per-link RSRP is bit-identical across RNG seeds, the offline tool skips recompute on an unchanged request via a request-hash cache, and Sionna runs reproduce from their own pinned fingerprint baselines — while the default build stays byte-for-byte unaffected.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: MOD-02, TOOL-05, REP-01, REP-02
**Success Criteria** (what must be TRUE):

  1. When the Sionna model is active, statistical shadowing, the random LOS draw, and the 38.901 penetration loss are not applied on top of the Sionna path gain (audited and suppressed at the path-gain source).
  2. A two-seed determinism test confirms per-link RSRP/path-gain is bit-identical across RNG seeds, proving no statistical term leaks into the static geometry.
  3. The offline tool caches by a hash of the full request (scene, materials, antennas, positions, carrier, numerology, band count, Tx power, MCS set, RT settings, transform); a rerun with the same request skips Sionna, while changing any physical input invalidates the cache and a non-physical input (timestamp/path) does not.
  4. Sionna configurations reproduce from their own pinned fingerprint baselines against the committed artifact (never matched to the statistical model's baselines), and reproducibility comes from the pinned artifact rather than a re-run of the GPU precompute.

**Plans**: TBD

### Phase 4: Differentiators (Authored Scene, Path-Gain RSRP, Auto-Invoke, Calibration Report)

**Goal**: On top of the validated, calibrated core, a researcher can run a real-map (Munich) scene that demonstrates site-specific propagation Simu5G cannot produce analytically, get Sionna-derived RSRP, optionally let the sim auto-produce a missing artifact, and read a bounded-difference calibration report — all strictly additive, with Python never becoming a default-run dependency.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: DIF-01, DIF-02, DIF-03, DIF-04
**Success Criteria** (what must be TRUE):

  1. A researcher runs an authored real-map scene (e.g. the built-in Munich scene) and observes site-specific LOS/NLOS propagation that the analytic model cannot reproduce.
  2. `getRSRP()` / `getRSRP_D2D()` return the Sionna per-link path gain (path-gain-based RSRP).
  3. With an opt-in ini flag, the simulation spawns the precompute tool when the artifact is missing; with the flag off, a normal run has no Python dependency.
  4. A bounded-difference calibration report artifact is produced comparing Sionna vs. Friis vs. 3GPP, with the residual quantified and explained.

**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Thin End-to-End Slice | 1/4 | In Progress|  |
| 2. Full BLER Table & CQI Consistency | 0/TBD | Not started | - |
| 3. Correctness Hardening | 0/TBD | Not started | - |
| 4. Differentiators | 0/TBD | Not started | - |
