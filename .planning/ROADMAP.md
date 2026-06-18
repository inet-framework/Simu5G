# Roadmap: Simu5G × Sionna RT Integration

## Overview

This roadmap delivers an optional, opt-in `SionnaChannelModel` for Simu5G as a sequence of
**end-to-end vertical slices** (Vertical MVP), not isolated technical layers. The first phase is a
thin, complete bring-up-and-validation slice: a minimal shared scenario (SSOT) drives a minimal
offline Sionna tool over an empty world with a single link, producing a minimal HDF5 artifact +
manifest, which a minimal `SionnaChannelModel` loads and runs a single-link simulation against —
proven correct by the empty-world Friis round-trip check.

**Plan A (channel) pivot — 2026-06-18.** After Phase 1, the roadmap prioritizes maturing the
**channel** track (Plan A) before the BLER track (Plan B): Phase 2 matures the channel source &
format (dual-source invocation = offline artifact + opt-in external subprocess; a versioned JSON
exact-float channel table; a pluggable `SionnaManager` source strategy — the seam toward dynamics);
Phase 3 adds the `CompareChannelModel` validation decorator plus hardening (no-double-counting,
two-seed determinism, request-hash cache, pinned fingerprints); Phase 4 matures the scene
(free-space → two-ray → real map), path-gain RSRP, the `interferenceMode`/`granularity` params, and
the calibration report. The **BLER table with CQI/feedback consistency is parked to Phase 5** and
resumes after the channel decisions land (it extends the same `SionnaTable`/JSON artifact). The artifact's
`schema_version` + `coord_transform` contract and the fail-loud parameter assertion are foundational
and ship in minimal form inside Phase 1 because every later slice extends them; the v1 noise-limited
table is designed as a strict subset of v2 (degenerate SINR-bin axis) from day one. v2 items
(interference curves, mobility, per-RB MIMO, INET-scene derivation) are explicitly out of this roadmap.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Thin End-to-End Slice (Bring-Up & Empty-World Validation)** - One SSOT → one empty-world link → one HDF5 artifact → one loaded `SionnaChannelModel` run, validated by the Friis round-trip; default build provably Python/HDF5-free. (completed 2026-06-17)
- [ ] **Phase 2: Channel Source & Format Maturation** - Dual-source invocation (offline artifact + opt-in external Sionna subprocess) + a versioned JSON exact-float channel table/exchange format + a pluggable `SionnaManager` source strategy (the seam toward dynamics).
- [ ] **Phase 3: Channel Validation & Hardening** - `CompareChannelModel` decorator (built-in vs Sionna, RNG-neutral per-link deltas), no-double-counting suppression, two-seed determinism, request-hash cache, and pinned Sionna fingerprint baselines.
- [ ] **Phase 4: Scene & Differentiators** - Flat-ground/two-ray → authored real-map scene, Sionna-derived RSRP, `interferenceMode`/`granularity` params + coupling guard, and a bounded-difference calibration report.
- [ ] **Phase 5: BLER Table & CQI/Feedback Consistency (PARKED)** - Deferred Plan B track: per-(link,MCS) BLER via `PHYAbstraction`; one table drives reception and CQI feedback with a self-consistency gate. Pre-pivot plans archived in `.planning/parked/`.

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
- [x] 01-02-PLAN.md — Build isolation: Simu5G_Sionna .oppfeatures feature + compilable C++/NED skeleton + default-binary symbol-check gate (SEAM-02)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-03-PLAN.md — C++ consumer: ManifestReader/SionnaTable load+validate, SionnaManager fail-loud contract assertion, SionnaChannelModel::getAttenuation (MOD-01, CAL-02, ART-01)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-04-PLAN.md — End-to-end run: single-link network + ini selection, Friis round-trip closes in-sim, pinned fingerprint baseline (SEAM-01, MOD-01, CAL-01)

### Phase 2: Channel Source & Format Maturation

**Goal**: Mature the channel pipeline (Plan A): the C++ side can obtain the channel either by loading a pre-generated offline artifact (default) or via an opt-in external Sionna subprocess; the channel table/exchange is a versioned JSON exact-float format that both Python paths emit identically; and `SionnaManager` owns a pluggable source strategy so static-vs-future-dynamic is a manager concern, not a model change — all while the default build/run stays Python-free.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: TOOL-06, ART-03, MOD-04
**Success Criteria** (what must be TRUE):

  1. `channelSource = artifact | subprocess | auto` works: default loads the cached artifact; `auto` loads cache and spawns the **external** Sionna subprocess on a miss; a missing artifact with spawn disabled fails loudly (`cRuntimeError`); the default binary still links no Python/HDF5 symbols (SEAM-02 holds).
  2. The channel table/exchange is a **versioned JSON with round-trip-exact float repr** (`%.17g`/hex); the offline tool and the subprocess emit byte-identical format; the C++ side reads it and a re-emit round-trips the path-gain values bit-exact.
  3. There is **one** `SionnaChannelModel` (thin reader); `SionnaManager` selects a pluggable source strategy (`StaticArtifact | Subprocess`) by parameter, and switching source is config — the model's `getSINR`/`getAttenuation`/`getRSRP` are unchanged.

**Plans**: TBD

### Phase 3: Channel Validation & Hardening

**Goal**: Validate the Sionna channel against the built-in analytic model with a live, apples-to-apples per-link decorator, and harden correctness: statistical path-loss terms fully suppressed (Sionna owns path gain), per-link RSRP bit-identical across RNG seeds, the offline tool skips recompute via a request-hash cache, and Sionna runs reproduce from their own pinned fingerprint baselines — while the default build stays byte-for-byte unaffected.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: CAL-03, MOD-02, TOOL-05, REP-01, REP-02
**Success Criteria** (what must be TRUE):

  1. A `CompareChannelModel : LteChannelModel` decorator runs the built-in model and `SionnaChannelModel` on identical inputs and emits per-link attenuation/RSRP/SINR deltas (bias, RMSE, correlation) as vec/sca; with default `primary` = built-in the deterministic Sionna side draws no RNG and fingerprints are unperturbed.
  2. When the Sionna model is active, statistical shadowing, the random LOS draw, and the 38.901 penetration loss are not applied on top of the Sionna path gain (audited and suppressed at the path-gain source).
  3. A two-seed determinism test confirms per-link RSRP/path-gain is bit-identical across RNG seeds, proving no statistical term leaks into the static geometry.
  4. The offline tool caches by a hash of the full request (scene, materials, antennas, positions, carrier, numerology, band count, Tx power, MCS set, RT settings, transform); a rerun with the same request skips Sionna, while changing any physical input invalidates the cache and a non-physical input (timestamp/path) does not.
  5. Sionna configurations reproduce from their own pinned fingerprint baselines against the committed artifact (never matched to the statistical model's baselines).

**Plans**: TBD

### Phase 4: Scene & Differentiators

**Goal**: On top of the validated, calibrated core, mature the scene from pure free-space to flat-ground/two-ray and then an authored real-map scene that shows site-specific propagation Simu5G cannot produce analytically; return Sionna-derived RSRP; expose the `interferenceMode`/`granularity` parameters (+coupling guard) as the forward seam toward multi-cell; and produce a bounded-difference calibration report — all strictly additive, Python never a default-run dependency.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: DIF-01, DIF-02, MOD-05, DIF-04
**Success Criteria** (what must be TRUE):

  1. The scene matures from free-space to a flat-ground/two-ray model and then an authored real-map scene (e.g. the built-in Munich scene), showing site-specific LOS/NLOS propagation the analytic model cannot reproduce; the CAL-01 free-space Friis anchor is retained as a separate sanity check.
  2. `getRSRP()` / `getRSRP_D2D()` return the Sionna per-link path gain (path-gain-based RSRP).
  3. `SionnaManager` exposes `interferenceMode` (noise-limited|all-pairs) and `granularity` (per-RB|wideband) with a coupling guard that rejects/warns the inconsistent wideband+all-pairs combo; the default remains noise-limited + per-RB.
  4. A bounded-difference calibration report artifact is produced comparing Sionna vs. Friis vs. 3GPP, with the residual quantified and explained.

**Plans**: TBD

### Phase 5: BLER Table & CQI/Feedback Consistency (PARKED)

**Goal**: The offline tool produces BLER for every CQI/MCS per link via `sionna.sys.PHYAbstraction`, and both the reception path and the CQI-feedback path read that one `SionnaTable`, so the scheduler's chosen MCS and the realized BLER provably agree.
**Mode:** mvp
**Depends on**: Phase 2 (the BLER table extends the same `SionnaTable` + JSON artifact, so it resumes after the channel source/format decisions land)
**Requirements**: TOOL-04, MOD-03, FB-01
**Status**: **PARKED** (Plan A pivot, 2026-06-18). The pre-pivot research and plans are archived under `.planning/parked/02-full-bler-table-cqi-feedback-consistency/` (CONTEXT, RESEARCH, PATTERNS, VALIDATION + 4 PLANs). Expect a re-plan when resumed, since the channel JSON/source changes affect the shared `SionnaTable`.
**Success Criteria** (what must be TRUE):

  1. The artifact holds a BLER value for all CQIs/MCS per link, computed via `sionna.sys.PHYAbstraction` (effective-SINR → shipped 5G-NR LDPC reference tables, EESM) with a pinned `mcs_table_index`.
  2. Reception decisions use the `SionnaTable` BLER lookup while retaining the `uniform(0,1) ≤ BLER` success draw and the `harqReduction_` HARQ heuristic on top.
  3. `SionnaFeedbackComputation::getCqi()` selects the highest MCS with BLER ≤ `targetBler_` from the **same** `SionnaTable`, with an init self-consistency check.
  4. A controlled-link test shows scheduler MCS and realized BLER move together.

**Plans**: Parked (archived).

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4. Phase 5 (BLER track) is **parked** and resumes after the channel track.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Thin End-to-End Slice | 4/4 | Complete    | 2026-06-18 |
| 2. Channel Source & Format Maturation | 0/TBD | Not started | - |
| 3. Channel Validation & Hardening | 0/TBD | Not started | - |
| 4. Scene & Differentiators | 0/TBD | Not started | - |
| 5. BLER Table & CQI/Feedback | 0/4 | Parked | - |
