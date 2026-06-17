# Requirements: Simu5G × Sionna Integration

**Defined:** 2026-06-17
**Core Value:** Simu5G can opt in to site-specific, geometry-derived channel/BLER from Sionna RT without changing the default build or behavior.

## v1 Requirements

Requirements for the initial reusable module. Each maps to a roadmap phase.

### Seam & Build Isolation

- [ ] **SEAM-01**: User can select the Sionna model opt-in via an ini string
  (`lteChannelModelType` / `nrChannelModelType` = `SionnaChannelModel`), with no NED
  interface change.

- [x] **SEAM-02**: A default Simu5G build and a normal simulation run are byte-for-byte
  unaffected — no Python / TensorFlow / PyTorch / GPU / HDF5 dependency in a normal build.

### Offline Sionna Precompute Tool

- [x] **TOOL-01**: A shared scenario description (SSOT) defines positions, antenna
  heights/orientation, carrier frequency, numerology, band count, materials, and the MCS set;
  it drives both the precompute tool and the simulation.

- [x] **TOOL-02**: The tool owns an explicit, versioned coordinate/units transform
  (origin, axes/handedness, scale, units) between OMNeT++ coordinates and the Sionna scene.

- [x] **TOOL-03**: The tool runs one batched `PathSolver` over all Tx/Rx pairs and extracts
  per-link path gains (`Paths.cfr` over `subcarrier_frequencies`).

- [ ] **TOOL-04**: The tool computes BLER per (link, MCS) for all CQIs/MCS via
  `sionna.sys.PHYAbstraction` (effective-SINR → shipped 5G-NR LDPC reference tables, EESM).

- [ ] **TOOL-05**: The tool precomputes once and caches by a hash of the full request
  (scene, materials, antennas, positions, carrier, numerology, band count, Tx power, MCS set,
  RT settings, transform); a rerun with the same request skips Sionna.

### Artifact

- [x] **ART-01**: The tool emits an HDF5 artifact + JSON manifest sidecar carrying a
  `schema_version` field checked at load time.

- [x] **ART-02**: The manifest carries the full parameter contract, the `coord_transform`
  block, and the request hash; the schema reserves a degenerate SINR-bin axis so v2
  interference curves are an additive extension.

### Channel Model (C++ consumer)

- [x] **MOD-01**: `SionnaChannelModel : LteRealisticChannelModel` retains the inherited
  `getSINR()` interference + noise aggregation — Simu5G owns the SINR value.

- [ ] **MOD-02**: When the Sionna model is active it fully owns path gain; Simu5G's
  statistical shadowing, random LOS draw, and 38.901 penetration loss are suppressed
  (no double-counting).

- [ ] **MOD-03**: BLER comes from a `SionnaTable` lookup, keeping the `uniform(0,1) ≤ BLER`
  success draw and the `harqReduction_` HARQ heuristic on top.

- [ ] **FB-01**: `SionnaFeedbackComputation::getCqi()` selects the highest MCS with
  BLER ≤ `targetBler_` from the **same** `SionnaTable` (one table, two readers); an init
  self-consistency check confirms scheduler MCS and realized BLER agree.

### Validation & Reproducibility

- [x] **CAL-01**: A reference / empty-world (obstacle-free) calibration mode validates the
  Sionna path gain against the Friis free-space formula at the same distance within a
  physically justified tolerance (~0.5–1 dB vs. free-space, not vs. the 3GPP formula).

- [x] **CAL-02**: `SionnaManager` asserts the artifact manifest against the live scenario at
  init and fails loudly (`cRuntimeError`) on any parameter-contract mismatch.

- [ ] **REP-01**: Sionna configurations have their own pinned fingerprint baselines against
  the committed artifact (never matched to the statistical model's baselines).

- [ ] **REP-02**: A two-seed determinism test confirms per-link RSRP/path-gain is
  bit-identical across RNG seeds (catches leaked statistical terms).

### Differentiators (v1.x — additive, after calibration passes)

- [ ] **DIF-01**: An authored "some world" real-map scene (e.g. the built-in Munich scene)
  demonstrates site-specific propagation Simu5G cannot produce analytically.

- [ ] **DIF-02**: Path-gain-based RSRP — `getRSRP()` / `getRSRP_D2D()` return Sionna per-link
  path gain.

- [ ] **DIF-03**: Optional auto-invocation — the sim spawns the precompute tool if the
  artifact is missing, opt-in via a separate ini flag; Python never becomes a default-run
  dependency.

- [ ] **DIF-04**: A bounded-difference calibration report artifact (Sionna vs. Friis vs. 3GPP,
  with quantified, explained residual).

## v2 Requirements

Deferred to a future milestone. Tracked but not in the current roadmap.

### Interference & Dynamics

- **V2-01**: BLER-vs-SINR curves + path gains so Simu5G's dynamic interference moves the
  operating point (the v1 noise-limited table is a strict subset).

- **V2-02**: Dynamic transmit-power control support.
- **V2-03**: Mobility / time-varying channel (Doppler, per-path temporal evolution).

### Fidelity & Geometry

- **V2-04**: Full per-RB MIMO matrices + rank adaptation.
- **V2-05**: INET buildings/obstacle model → Sionna scene derivation (toward full
  single-source-of-truth geometry).

- **V2-06**: Full link-level LDPC Monte Carlo BLER backend (higher fidelity than EESM).
- **V2-07**: Per-redundancy-version HARQ BLER precompute (replacing the `harqReduction_`
  heuristic).

## Out of Scope

Explicitly excluded for v1. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Per-TTI / runtime coupling, embedded Python, runtime IPC | Precompute-once only; avoids runtime Python/GPU overhead and keeps determinism |
| TensorFlow-based Sionna code paths | Sionna 2.x is Dr.Jit/Mitsuba 3 + PyTorch; TF is not installed and must not be added |
| Exact numeric agreement with the 3GPP statistical formula | Different physics by construction; chasing it would destroy site-specific value — target a bounded, explained difference |
| Mobility, dynamic interference, dynamic power (v1) | Deferred to v2; v1 is a static, noise-limited strict subset |

## Traceability

Which phases cover which requirements. Filled during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| SEAM-01 | Phase 1 | Pending |
| SEAM-02 | Phase 1 | Complete |
| TOOL-01 | Phase 1 | Complete |
| TOOL-02 | Phase 1 | Complete |
| TOOL-03 | Phase 1 | Complete |
| ART-01  | Phase 1 | Complete |
| ART-02  | Phase 1 | Complete |
| MOD-01  | Phase 1 | Complete |
| CAL-01  | Phase 1 | Complete |
| CAL-02  | Phase 1 | Complete |
| TOOL-04 | Phase 2 | Pending |
| MOD-03  | Phase 2 | Pending |
| FB-01   | Phase 2 | Pending |
| MOD-02  | Phase 3 | Pending |
| TOOL-05 | Phase 3 | Pending |
| REP-01  | Phase 3 | Pending |
| REP-02  | Phase 3 | Pending |
| DIF-01  | Phase 4 | Pending |
| DIF-02  | Phase 4 | Pending |
| DIF-03  | Phase 4 | Pending |
| DIF-04  | Phase 4 | Pending |

**Coverage:**

- v1 requirements: 21 total (17 table stakes + 4 differentiators)
- Mapped to phases: 21 (Phase 1: 10, Phase 2: 3, Phase 3: 4, Phase 4: 4)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-06-17*
*Last updated: 2026-06-17 after roadmap creation*
