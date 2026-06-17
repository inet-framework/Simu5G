# Project Research Summary

**Project:** Simu5G × Sionna RT Integration
**Domain:** Offline ray-tracing channel/PHY-abstraction bridge (Python/GPU → cached artifact → C++/OMNeT++), precompute-once link-to-system, static scenarios
**Researched:** 2026-06-17
**Confidence:** HIGH

## Executive Summary

This project adds an optional, opt-in channel-model module to Simu5G that replaces the simulator's statistical 3GPP PHY abstraction with results derived from NVIDIA Sionna RT ray tracing over real 3D geometry. The integration is strictly a **two-process, one-artifact** architecture: a standalone Python offline tool runs Sionna RT ahead of time and writes a cached HDF5 table; the C++ simulation loads that table at init and performs read-only lookups at runtime. No Python, TensorFlow, PyTorch, or GPU dependency enters the Simu5G build or a normal simulation run. The critical technology fact that overrides training-data intuition: **Sionna 2.x is NOT TensorFlow** — Sionna RT 2.0.1 runs on Dr.Jit/Mitsuba 3, and `sionna.phy`/`sionna.sys` run on PyTorch (`torch 2.12.0`). TensorFlow is not installed in the verified venv and must not appear in any code.

The recommended internal pipeline is: `load_scene` → `PathSolver` → `Paths.cfr(subcarrier_frequencies(...))` → per-subcarrier post-eq SINR (noise-limited v1: SNR) → `sionna.sys.PHYAbstraction` (built-in EESM + shipped 5G-NR LDPC BLER tables) → BLER per (link, MCS). **`sionna.sys.PHYAbstraction` is the v1 BLER method** — it is not hand-rolled; it ships calibrated, and it removes the entire "where do reference curves come from" open question. The artifact format is HDF5 (`h5py 3.16.0`, already in venv) for the bulk numeric `(link, MCS)` BLER + path-gain table, with a tiny JSON manifest sidecar carrying the parameter contract and request hash. The schema must include a `schema_version` field and a `coord_transform` block from day one; these are the two surfaced gaps in PROJECT.md that must be explicit artifacts, not implicit conventions.

The dominant risk theme across all four research files is **silent corruption that still looks plausible**: position/unit/scale errors, dB/linear mix-ups, SINR x-axis convention drift, double-counted path loss, and stale-cache bugs all produce simulations that run, return numbers in plausible ranges, and are wrong. Every guard against this class of failure must be automated, not eyeballed: an empty-world Friis round-trip check (~0.5–1 dB tolerance vs. free-space, not vs. 3GPP statistics), a two-seed identical-RSRP determinism test, a fail-loud parameter-contract assertion at init, and a self-consistency check that the MCS selected by the feedback path has realized BLER ≤ target on the same Sionna table. The agreed calibration tolerance is set vs. **shared physics (Friis ≈ 0.5–1 dB)**, not vs. the 3GPP statistical formula — exact numerical agreement with the 3GPP curve is the wrong target and would destroy site-specific value if chased.

---

## Key Findings

### Recommended Stack

The offline tool lives entirely in the existing sibling venv (`/home/zoli/Projects/OMNET/Sionna/venv`); the Simu5G C++ binary gains only a small HDF5 reader (header-only HighFive, or raw HDF5 C lib, or — if zero HDF5 in C++ is required — a versioned little-endian binary table paired with the JSON manifest). All versions are pinned and verified in the installed venv; do not re-resolve dependencies.

**Core technologies (offline tool venv only):**
- **sionna-rt 2.0.1** (Dr.Jit + Mitsuba 3, TF-free): ray tracing, scene load, `PathSolver`, `Paths.cfr`, `RadioMapSolver` — the RT engine
- **sionna 2.0.1** (PyTorch): `sionna.sys.PHYAbstraction` (EESM + shipped 5G-NR LDPC BLER tables) — the complete effective-SINR→BLER step; do not hand-roll
- **mitsuba 3.8.0**: scene geometry/material backend (auto-selected variant; do not override)
- **drjit 1.3.1**: JIT/autodiff array backend; LLVM for CPU, CUDA for GPU — transparent
- **torch 2.12.0**: backend for sionna.phy/sys; replaces TensorFlow entirely
- **numpy 2.4.6**: array glue; `Paths.cfr(..., out_type="numpy")` hands back plain arrays
- **h5py 3.16.0**: artifact writer (offline tool) and reader (C++ side via HighFive or raw HDF5 C lib)
- **Python 3.13.7**: host language (≥3.11 required by sionna.phy/sys)

**C++ side (Simu5G):** new `src/simu5g/sionna/` subdir + `SionnaChannelModel`, `SionnaFeedbackComputation`, `SionnaManager`, `SionnaTable` — no new heavy dependencies in a default build.

### Expected Features

**Must have — table stakes (v1):** every item below is a correctness or usability gate; absence makes the module untrustworthy or unmergeable.

- **Opt-in `SionnaChannelModel : LteRealisticChannelModel`** — selected via `lteChannelModelType`/`nrChannelModelType` ini string, `like ILteChannelModel` NED polymorphism; **no NED interface change**; default build byte-for-byte unaffected; no Python/GPU in a normal run
- **Division of labor** — Sionna owns SINR→BLER (via `PHYAbstraction`); Simu5G owns SINR value (inherited `getSINR()` interference+noise aggregation); v1 noise-limited → one BLER per (link, MCS)
- **Artifact schema + versioning** [GAP] — `schema_version` field + load-time check; prerequisite for fail-loud and cache features being trustworthy across project evolution
- **Coordinate/units transform** [GAP] — explicit, named, version-pinned object (origin, axes, scale, units) owned by the offline tool; C++ side only asserts the manifest; single shared scenario source drives both processes; never hand-place nodes twice
- **Parameter-contract assertion, fail-loud at init** — assert artifact manifest == live scenario (carrier freq, numerology, band count, positions hash, Tx-power convention, SINR x-axis definition, `sionna_rt_version`); `cRuntimeError` on mismatch; never silently fall back
- **Precompute-once + request-hash cache** — hash covers scene mesh, per-surface materials, antenna arrays/patterns, positions/orientations, carrier, numerology, bandwidth/band count, Tx power, MCS set, RT settings (`max_depth`, sample counts), coordinate-transform object; excludes timestamps/paths; cache key embedded in artifact for re-verification
- **No double-counting** — `SionnaChannelModel` fully owns path gain when active; override `getAttenuation()`/`computePathLoss()` to bypass Simu5G's statistical shadowing, random LOS draw, and 38.901 penetration loss entirely
- **CQI-feedback consistency** — `SionnaFeedbackComputation::getCqi()` inverts the **same** SionnaTable (highest MCS with BLER ≤ `targetBler_`); table must hold BLER for **all** CQIs/MCS per link; one table, two readers (reception + feedback)
- **Reference/empty-world calibration mode** — obstacle-free Sionna world; Friis round-trip check within ~0.5–1 dB (vs. free-space, not vs. 3GPP formula); validates coordinate transform and link-budget convention before any site-specific scene is trusted
- **Own fingerprint baselines** — Sionna runs consume RNG differently (no per-link fading draws); must have their own baselines against the pinned artifact; never match the statistical model's baselines

**Should have — differentiators (v1.x, add after empty-world calibration passes):**
- **Authored "some world" real-map scene** (e.g. built-in Munich) — site-specific geometry, the headline capability Simu5G fundamentally lacks
- **Path-gain-based RSRP** — override `getRSRP()`/`getRSRP_D2D()` to return Sionna per-link path gain; low marginal cost (path gain already extracted for no-double-counting)
- **Bounded-difference calibration report artifact** — human-readable comparison (Sionna vs. Friis/3GPP, with quantified and explained residual); reproducible trust evidence for the community
- **Optional auto-invocation** from the sim to produce the artifact if missing; strictly additive; must not make Python a default-run dependency

**Defer (v2+):**
- Dynamic interference curves (BLER-vs-SINR + path gains) — v1 noise-limited subset must be validated first; v1 is a strict subset of v2, nothing wasted
- Full per-RB MIMO + rank adaptation; mobility/Doppler; INET-buildings → Sionna scene derivation; full LDPC Monte Carlo backend; dynamic Tx-power control; per-RV HARQ BLER precompute

### Architecture Approach

The integration spans two fully decoupled processes joined by a single cached artifact. The **offline `sionna_precompute` tool** (Python, in the existing venv) owns all RT physics and the coordinate/parameter transform; it reads a shared scenario description (SSOT), traces all Tx/Rx pairs in one batched Sionna run, computes effective-SINR→reference-curve BLER per (link, MCS) via `PHYAbstraction`, and emits a hash-keyed HDF5 artifact + JSON manifest. The **C++ side** adds only five new entities: `SionnaManager` (network-global module; loads the artifact once at init, asserts the manifest, owns the in-memory `SionnaTable`), `SionnaTable` (lookup structure: `(linkId, MCS) → BLER`, `linkId → pathGain`), `SionnaChannelModel : LteRealisticChannelModel` (per-PHY thin lookup; inherits `getSINR()` interference summation, overrides path-gain source and BLER lookup), and `SionnaFeedbackComputation` (CQI from the same table). Selection is a string parameter — no NED interface change. `SionnaManager` is a dedicated module (not extended `Binder`) to guarantee zero presence in default builds.

**Major components:**
1. **`sionna_precompute/` offline CLI** — RT trace, coord/param transform (owner), effective-SINR→BLER, cache, artifact + manifest emitter; lives in `Sionna/` venv, never linked into C++
2. **Cached artifact** (HDF5 + JSON manifest) — the only cross-process contract; schema-versioned, request-hash-keyed, pinned; `schema_version` + `coord_transform` attrs + full parameter contract
3. **`SionnaManager` + `SionnaTable`** — global owner: load-once at init, manifest assertion (fail-loud), read-only lookup API; one instance per network, referenced via `ModuleRefByPar`
4. **`SionnaChannelModel : LteRealisticChannelModel`** — overrides path-gain source and BLER lookup; keeps inherited `getSINR()` interference+noise aggregation and `uniform(0,1) ≤ BLER` success draw + `harqReduction_` heuristic
5. **`SionnaFeedbackComputation`** — `getCqi()` inverts the same SionnaTable; parallel to `LteFeedbackComputationRealistic`
6. **Shared scenario SSOT** — YAML/JSON; single source for positions, antenna heights, carrier, numerology, band count, materials, MCS set; drives both processes

**Key structural insight:** derive `SionnaChannelModel` from `LteRealisticChannelModel` (not abstract `LteChannelModel`), mirroring how `NrChannelModel` works, to inherit the full `getSINR()` interference+noise machinery and override only the two hooks (path-gain source in `getAttenuation()`/`computePathLoss()`; BLER lookup at `binder_->phyPisaData.getBler(...)` call site, `LteRealisticChannelModel.cc:1796`).

**v1-as-strict-subset extension point:** design `SionnaTable.lookupBler(linkId, mcs, sinr)` and the HDF5 schema with a SINR-bin axis present-but-degenerate in v1 (one bin); v2 fills the axis with curves + path gains and the `sinr` argument becomes active. No structural refactor needed.

### Critical Pitfalls

All ten identified pitfalls share the same failure mode: **silent corruption that still looks plausible**. The top five requiring automated guards before any result is trusted:

1. **Silent coordinate/units/scale mismatch** — node positions land in the wrong place in the Sionna scene (wrong axis, scale, origin offset, antenna height on wrong axis); the RT still returns finite path gains, the error is invisible. Guard: one shared SSOT generates both processes' geometry; explicit versioned transform object in the offline tool; empty-world Friis round-trip assertion at init (OMNeT++ Euclidean distance vs. Sionna LOS path gain, within ~0.5–1 dB); render-and-eyeball once per scene.

2. **Tx-power / path-gain convention mismatch (dB vs linear, double-counted antenna gain)** — Sionna's `g = Σ|aₙ|²` is a linear power gain with antenna patterns `C_T`/`C_R` baked in but Tx power not included; applying linear gain as dB, or double-counting antenna gain, produces BLER saturated at 0 or 1 with no exception. Guard: one-line link-budget contract string in the artifact manifest; assert it in the C++ loader; validate via empty-world Friis link budget (no clean 10/20/30 dB offsets).

3. **SINR x-axis definition mismatch** — the BLER curve x-axis does not match Simu5G's `signal/(interference+noise)`; throughput is biased by ~1–2 MCS steps everywhere. Guard: define the SINR x-axis in writing in the artifact manifest and honor it on both sides; assert that the computed operating SNR equals the curve anchor SNR at a known noise-limited LOS point.

4. **Double-counting deterministic RT path gain with statistical shadowing/LOS/penetration** — inheriting `LteRealisticChannelModel::getSINR()` without suppressing statistical terms causes RSRP to vary run-to-run with RNG seed despite static geometry. Guard: explicitly suppress all statistical path-loss terms; verify with a **two-seed test** (per-link RSRP must be bit-identical across seeds).

5. **CQI feedback and realized BLER from different tables** — `getCqi()` reading `phyPisaData` while `isReceptionSuccessful()` reads the Sionna table → scheduler picks MCS the Sionna channel cannot sustain. Guard: route both paths through the same `SionnaTable`; store BLER for **all** CQIs/MCS per link; self-consistency assertion at init.

---

## Implications for Roadmap

All four research files converge on the same phase ordering: contract and seam first, then offline physics producer, then online consumer with correctness guards, then calibration validation, then differentiators. v1 is a strict subset of v2 — nothing produced in any phase is thrown away.

### Phase 1: Contract, Seam & Build Isolation

**Rationale:** The artifact schema and NED/ini opt-in seam are referenced by every downstream phase. Build isolation (no Python/GPU in default build) is the core promise and must be proven before writing any C++ that touches Sionna data.

**Delivers:**
- Shared scenario SSOT format (YAML/JSON): positions, antenna heights, carrier, numerology, band count, materials, MCS set
- HDF5 artifact schema v1 with `schema_version`, `coord_transform` attrs, full parameter contract attrs, `request_hash`, `carrier_freq_hz`, `subcarrier_spacing_hz`, `num_bands`, `tx_power_dbm`, `effective_sinr_method`, `mcs_table_index`, `sionna_rt_version`; degenerate SINR-bin axis (`/sinr_grid`, `/bler_curve[L,M,1]`) for v2 compatibility; `/bler[L,M]` (v1 scalar); `/path_gain_db[L]`
- JSON manifest sidecar structure
- `SionnaChannelModel.ned` + `SionnaManager.ned` skeletons that compile cleanly
- CMake/Makefile guards proving no Python/HDF5 symbols in a default `make` build
- Stub `SionnaManager` that loads a test-fixture artifact and asserts its manifest; `SionnaChannelModel` stub selectable via ini; default run produces identical output to pre-integration baseline

**Addresses:** opt-in seam, artifact schema + versioning [GAP], build isolation

**Avoids:** Pitfalls 7 and 10 (fingerprint breakage and stale cache avoided by making schema and fail-loud contract explicit from day one)

**Research flag:** Standard patterns. NED seam is verified (file:line in ARCHITECTURE.md); HDF5 schema design is straightforward.

---

### Phase 2: Coordinate Transform & Offline RT Producer

**Rationale:** The coordinate/units transform is the single most likely source of plausible-but-wrong results and must be explicit and tested before any RT numbers are trusted. This phase also produces the first real artifact.

**Delivers:**
- `sionna_precompute/transform.py` — explicit, versioned `OMNeT2SionnaTransform` object (origin offset, axis permutation/handedness, scale); serialized into artifact `coord_transform` attrs
- `sionna_precompute/scenario.py` — SSOT loader + manifest builder
- `sionna_precompute/rt.py` — batched `PathSolver` run over all Tx/Rx pairs in one Sionna invocation; `Paths.cfr(subcarrier_frequencies(...), out_type="numpy")` → per-link path gains
- `sionna_precompute/schema.py` — artifact writer (HDF5 + JSON manifest)
- `sionna_precompute/cache.py` — `sha256(canonical serialized request)` keying; skip-on-hit
- Empty-world (obstacle-free) reference scene produces per-link path gains
- Sanity check: Sionna LOS path gain vs. OMNeT++ Euclidean Friis formula within ~0.5–1 dB
- `sionna_precompute/__main__.py` CLI entry point

**Addresses:** coordinate/units transform [GAP], precompute-once + request-hash cache, path-gain extraction (shared infra for no-double-counting and path-gain RSRP)

**Avoids:** Pitfall 1 (coordinate mismatch), Pitfall 9 (precompute cost blow-up — batched single run), Pitfall 10 (cache-key correctness)

**Research flag:** Standard patterns. Sionna RT API fully verified against installed venv (see STACK.md). `PathSolver.__call__`, `Paths.cfr`, `subcarrier_frequencies` signatures confirmed.

---

### Phase 3: Link-to-System BLER Pipeline & Parameter Assertion

**Rationale:** With path gains available, add the effective-SINR → BLER step via `sionna.sys.PHYAbstraction` and wire the fail-loud parameter assertion in `SionnaManager`. This phase closes the offline tool and produces a complete, loadable artifact.

**Delivers:**
- `sionna_precompute/link_to_system.py` — `PHYAbstraction(mcs_index, sinr=per_subcarrier_snr)` → BLER/TBLER per (link, MCS); EESM default; `mcs_table_index` pinned; writes `/bler[L,M]` and `/path_gain_db[L]` for all CQIs/MCS per link
- Documented SINR x-axis contract: "post-eq average SNR of white noise, antenna patterns included, Tx power not included; maps onto Simu5G's `signal/(interference+noise)` in v1 noise-limited cut"
- `SionnaManager` (C++): loads artifact at init, asserts all contract fields vs. live scenario, `cRuntimeError` on any mismatch; `SionnaTable.lookupBler(linkId, mcs, sinr)` API (sinr arg ignored in v1)
- Mismatched-manifest test: confirm init throws on any contract field difference
- Round-trip test: offline tool produces artifact → `SionnaManager` loads → asserts pass on matching scenario; rerun with same request hash skips Sionna

**Addresses:** division of labor + effective-SINR→curve BLER, parameter-contract assertion fail-loud

**Avoids:** Pitfall 2 (Tx-power/convention, via contract string), Pitfall 3 (SINR x-axis, via documented contract), Pitfall 6 (effective-SINR approximation documented as a phase deliverable)

**Research flag:** Standard patterns. `PHYAbstraction.__call__` signature and shipped BLER table behavior verified in installed venv. EESM is the default; MIESM available if empty-world calibration shows bias at high-order QAM.

---

### Phase 4: SionnaChannelModel Reception Path, No-Double-Counting & CQI Consistency

**Rationale:** Wire the C++ consumption side. The three correctness requirements (no double-counting, CQI consistency, SINR-x-axis contract) must land together because they all share the `SionnaTable` object and must all be consistent before any simulation result is meaningful.

**Delivers:**
- `SionnaChannelModel : LteRealisticChannelModel` (C++): override `getAttenuation()`/`computePathLoss()` to return Sionna per-link path gain (suppressing statistical shadowing, LOS draw, 38.901 penetration loss); override BLER lookup at `LteRealisticChannelModel.cc:1796` with `SionnaTable.lookupBler(linkId, mcs)`; keep inherited `getSINR()` aggregation, `uniform(0,1) ≤ BLER` success draw (`:1819`), `harqReduction_^(txNum-1)` heuristic (`:1817`)
- `SionnaFeedbackComputation` (C++): `getCqi()` inverts `SionnaTable` (highest MCS with BLER ≤ `targetBler_`)
- Self-consistency assertion at init: for each link, MCS selected by `getCqi()` has realized BLER ≤ `targetBler_` on the same table
- **Two-seed determinism test:** per-link RSRP/path-gain must be bit-identical across RNG seeds; any RSRP variance → statistical term leaking → test fails
- Regression test: default build produces byte-for-byte identical output to pre-integration baseline

**Addresses:** no double-counting, CQI-feedback consistency

**Avoids:** Pitfall 4 (double-counting, via suppression + two-seed test), Pitfall 5 (table disagreement, via single `SionnaTable`)

**Research flag:** Needs phase-planning audit. Before implementing, audit the full `LteRealisticChannelModel` inherited call tree to identify every code path injecting statistical path-loss terms.

---

### Phase 5: Empty-World Calibration & Fingerprint Baselines

**Rationale:** Calibration is the project's primary validation anchor. It must be a named phase with explicit deliverables because it gates trust in all site-specific results and sets the defensible tolerance band.

**Delivers:**
- Parameterizable empty-world scene (no obstacles, LOS-only) in the offline tool
- Comparison harness: Sionna path gain + Tx power (dBm) vs. textbook Friis formula at the same OMNeT++ Euclidean distance; tolerance ~0.5–1 dB; passing this means coordinate transform, link-budget convention, and dB/linear handling are all correct
- Documented tolerance rationale: tolerance is vs. **free-space Friis** (same physics), not vs. the 3GPP statistical formula (different physics by construction); 3GPP residual is quantified and explained
- Sionna-specific fingerprint baselines pinned against the committed empty-world artifact; CI passes only when the pinned artifact is loaded (never re-generated from GPU)
- **Automated "looks done but isn't" test suite:** Friis round-trip (Pitfalls 1+2), two-seed identical RSRP (Pitfall 4), CQI self-consistency (Pitfall 5), SINR x-axis spot-check (Pitfall 3), cache-invalidation correctness (Pitfall 10)

**Addresses:** reference/empty-world calibration mode, own fingerprint baselines

**Avoids:** Pitfall 7 (fingerprint drift), Pitfall 8 (false equality expectation)

**Research flag:** Tolerance-band decision needed before phase gate. The ~0.5–1 dB vs. Friis estimate must be confirmed empirically on the first real empty-world run.

---

### Phase 6: Differentiators — Authored Scene, Path-Gain RSRP, Auto-Invoke

**Rationale:** These features motivate opting in over the validated analytic default. All depend on the complete, calibrated v1 pipeline. All are strictly additive — nothing in Phases 1–5 is changed.

**Delivers:**
- **Authored "some world" real-map scene:** standalone Sionna scene (e.g. `sionna.rt.scene.munich` or synthetic geometry); demonstrates site-specific propagation that Simu5G cannot produce analytically
- **Path-gain-based RSRP:** override `getRSRP()`/`getRSRP_D2D()` (pure-virtuals at `LteChannelModel.h:94,101`) to return Sionna per-link path gain from `SionnaTable`; low marginal cost
- **Bounded-difference calibration report:** human-readable comparison artifact (Sionna vs. Friis vs. 3GPP, with quantified and explained residual)
- **Optional auto-invocation:** sim spawns `sionna_precompute` if artifact is missing; opt-in via separate ini flag; preserves "no Python in a normal run" guarantee

**Addresses:** authored scene, path-gain RSRP, calibration report [GAP], auto-invocation

**Research flag:** Standard patterns. RSRP override seam verified. Sionna built-in scenes and Blender/Mitsuba authoring documented.

---

### Phase Ordering Rationale

- **Contract/schema first:** referenced by every downstream phase; retrofitting a schema field is painful and risks regenerating all artifacts
- **Offline producer before C++ consumer:** the consumer cannot be tested without a real artifact; the producer is independently testable against Friis
- **Path-gain extraction in Phase 2** (not Phase 4): shared infrastructure for both no-double-counting and path-gain RSRP
- **Calibration after the full pipeline (Phase 5):** validates the complete chain; running earlier produces noise
- **Differentiators last (Phase 6):** gating on calibration prevents building a demo on an uncalibrated foundation
- **v1 is a strict subset of v2:** `lookupBler(linkId, mcs, sinr)` API and degenerate SINR-bin axis designed from Phase 1

### Research Flags

Needs audit/research: Phase 4 (`LteRealisticChannelModel` inherited call-tree audit for statistical terms), Phase 5 (calibration tolerance-band empirical confirmation).
Standard patterns: Phases 1, 2, 3, 6.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All Sionna RT/PHY/SYS APIs verified against installed 2.0.1 packages; "Sionna 2.x is PyTorch, not TF" confirmed by venv introspection. |
| Features | HIGH | Grounded in verified Simu5G source (file:line), PROJECT.md decisions, verified Sionna API. GAP items explicit and addressed. |
| Architecture | HIGH | Seam verified at file:line (`LteNicBase.ned:101`, `LteRealisticChannelModel.cc:1796`, `LteFeedbackComputationRealistic.cc:24-106`, `Binder.h:448`, `LteChannelModel.h:94,101`). |
| Pitfalls | HIGH (structural) / MEDIUM (calibration tolerances) | Structural pitfalls follow from Sionna's path-coefficient math + verified Simu5G pipeline. Tolerances must be measured. |

**Overall confidence:** HIGH

### Gaps to Address

- **Explicit coordinate/units transform specification** [GAP] — promote to explicit artifact + assertion. Phase 1 (schema attrs) + Phase 2 (`transform.py` + Friis round-trip).
- **Artifact schema + version field** [GAP, plan open-question #7] — `schema_version` + load-time check. Phase 1.
- **Calibration tolerance band** [open question] — confirm ~0.5–1 dB vs. Friis empirically. Phase 5 gate deliverable.
- **`LteRealisticChannelModel` statistical-term audit** — audit inherited call tree before Phase 4.
- **HARQ heuristic adequacy** [plan open-question #5] — keep `harqReduction_` in v1; promote per-RV BLER only if calibration shows bias.

---

## Sources

### Primary (HIGH confidence)
- Installed venv introspection (`/home/zoli/Projects/OMNET/Sionna/venv`) — verified versions (`sionna-rt 2.0.1`, `sionna 2.0.1`, `mitsuba 3.8.0`, `drjit 1.3.1`, `torch 2.12.0`, `numpy 2.4.6`, `h5py 3.16.0`, TensorFlow NOT installed) and exact signatures of `PathSolver`, `Paths.cfr/cir`, `subcarrier_frequencies`, `RadioMapSolver`, `PHYAbstraction`
- Simu5G source (file:line): `ILteChannelModel.ned:22`, `LteNicBase.ned:48,101`, `NrNicUe.ned:33,71`, `LteChannelModel.h:85-156`, `LteRealisticChannelModel.cc:509-518,1796,1817,1819`, `LteFeedbackComputationRealistic.cc:24,83-106`, `PhyPisaData.h:44`, `NrChannelModel.h:20`, `Binder.h:448`
- `.planning/PROJECT.md`; `Sionna/sionna-integration-plan.md` §§3–9

### Secondary (MEDIUM confidence)
- [Sionna RT Technical Report (arXiv 2504.21719)](https://arxiv.org/pdf/2504.21719) — Dr.Jit/Mitsuba rewrite, path-coefficient math `aₙ = (λ/4π)·C_R·(∏Tₙ)·C_T`
- [Sionna Documentation 2.0.1](https://nvlabs.github.io/sionna/) — RT API, SYS PHYAbstraction
- [New Radio PHY Abstraction for 5G-NR (arXiv 2001.10309)](https://arxiv.org/pdf/2001.10309) — EESM/MIESM link-to-system methodology
- [TensorFlow GPU non-determinism RFC](https://github.com/tensorflow/community/blob/master/rfcs/20210119-determinism.md)

### Tertiary (LOW confidence / needs validation)
- Calibration tolerance ~0.5–1 dB vs. Friis — must be measured on first empty-world run
- EESM vs. MIESM — EESM default; validate during calibration

---
*Research completed: 2026-06-17*
*Ready for roadmap: yes*
