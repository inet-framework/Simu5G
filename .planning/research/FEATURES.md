# Feature Research

**Domain:** Opt-in ray-tracing-derived channel/PHY module for a system-level 5G NR simulator (Simu5G × NVIDIA Sionna RT), precompute-once link-to-system, static scenarios
**Researched:** 2026-06-17
**Confidence:** HIGH (grounded in PROJECT.md decisions, the design plan, and verified Simu5G source anchors; external RT/link-to-system methodology is standard and stable)

> Scope note: this is not a 5G-simulator feature survey. The "product" is a reusable, opt-in channel-model module. A feature is "table stakes" if its absence makes the module **untrustworthy** (silently wrong) or **unusable** (breaks the default build/workflow), not merely incomplete. Categories below are aligned to the Active / Out-of-Scope lists in PROJECT.md; gaps those lists miss are flagged with **[GAP]**.

## Feature Landscape

### Table Stakes (Required or the Module Is Untrustworthy/Unusable)

These are non-negotiable for v1. They map 1:1 onto PROJECT.md "Active" requirements plus a few enabling capabilities those requirements imply.

| Feature | Why Expected (trust/usability) | Complexity | Notes |
|---------|--------------------------------|------------|-------|
| **Opt-in `SionnaChannelModel : LteChannelModel`** via NED `like ILteChannelModel` + ini; default build/behavior byte-for-byte unaffected; no Python/TF/GPU in a normal build | If selecting Sionna changes the default build or links Python into a stock run, the module is unusable and unmergeable. This is the core promise. | MEDIUM | Polymorphic seam already exists (`ILteChannelModel.ned` verified; `LtePhyBase` `channelModel[]`). Subclass `LteRealisticChannelModel`/`NrChannelModel` so SINR aggregation is inherited. Build isolation (no TF link) is the real work, not the NED wiring. |
| **Parameter-contract assertion + fail-loud at init** | A silent carrier/numerology/band-count/Tx-power/SINR-x-axis/polarization mismatch corrupts every result while looking plausible. Without this, no result can be trusted. | MEDIUM | Assert loaded artifact metadata == module's `getCarrierFrequency()`/`getNumBands()`/`getNumerologyIndex()` (verified these accessors exist on the base). Must cover the *convention* set in PROJECT.md Constraints, not just scalar values. Highest trust-per-line-of-code feature. |
| **Precompute-once + cache keyed by hash of the full request** (scene, positions, materials, antennas, freqs, powers, MCS set) | Without caching, every parameter-study run and every fingerprint rerun re-invokes Sionna → slow, and (with GPU/TF float nondeterminism) non-reproducible. Caching is what makes "invoke once" cheap and deterministic. | MEDIUM | Hash must include *everything* that changes the channel, or stale-cache returns wrong physics silently. Cache key design is a correctness feature, not just performance. Pin/commit the artifact for fingerprint stability. |
| **Reference / empty-world calibration mode** | The only way to detect a silent unit/convention mismatch before trusting site-specific output. An obstacle-free Sionna world should reproduce Simu5G's 3GPP formulas within a *bounded, explainable* difference. | MEDIUM-HIGH | Mandatory per PROJECT.md Key Decisions. Needs a parameterizable empty scene + a documented comparison harness + an agreed tolerance band. Honesty: exact numerical match is *not* the goal (PROJECT.md Constraint). This is the project's primary validation anchor. |
| **CQI-feedback consistency with the same BLER table** | If the scheduler picks MCS from one table and realized BLER comes from another, MCS and BLER disagree → the run is internally incoherent and untrustworthy. | MEDIUM | Rewire `LteFeedbackComputationRealistic::getCqi()` (verified: takes `targetBler_`, returns `Cqi`) to invert the *Sionna* table: highest MCS with BLER ≤ target. Forces the table to hold BLER for **all** CQIs/MCS per link, not just the chosen one — a schema requirement. |
| **No double-counting: Sionna fully owns path gain when active** | Applying Simu5G's statistical shadowing / random LOS draw / 38.901 penetration loss on top of deterministic RT gain double-counts loss → physically wrong. | MEDIUM | Override the path-loss/shadowing contributions so the inherited `getSINR()` interference+noise aggregation runs on Sionna path gains only. The interference *summation* is kept (Simu5G's strength); only the gain *source* changes. |
| **Own fingerprint baselines for Sionna configs** | RNG-stream consumption differs (no per-link fading draws), so Sionna runs will never match existing baselines. Without their own baselines, CI is red and the module looks broken. | LOW-MEDIUM | Mechanically adding baselines is low effort; ensuring the cached table is pinned so baselines stay stable is the real requirement. Depends on caching + determinism being solid first. |
| **Division of labor: Sionna owns SINR→BLER, Simu5G owns SINR value** (v1: noise-limited → one BLER per (link, MCS)) | This is the architectural contract that makes everything else coherent and keeps v1 a strict subset of v2. Get it wrong and the table schema / lookup are wrong everywhere. | MEDIUM | v1 collapses the curve to a single BLER per (link, freq, MCS) since SINR=SNR is static. Replaces `PhyPisaData::getBler(txMode,cqi,sinr)` (verified `[3][15][49]` table) with a per-link lookup. Keep `uniform(0,1) ≤ BLER` success draw + `harqReduction_` heuristic on top. |
| **Effective-SINR → reference-curve BLER method** | The chosen internal fidelity/cost point. Needed for the precompute to be affordable and iterable in v1. | MEDIUM-HIGH | PROJECT.md picks this over full LDPC Monte Carlo. Most of the *physics-correctness risk* concentrates here (effective-SINR mapping choice, interference-as-white assumption). Flag for deeper phase research. |
| **[GAP] Coordinate / units transform between OMNeT++ playground and the Sionna scene** | The plan (§6) calls a silent geometry mismatch "corrupts everything while still looking plausible," yet PROJECT.md's Active list does not name an explicit transform feature. It is implied by "shared scenario source" but deserves to be its own asserted, tested artifact (origin, axes, units, scale, antenna heights/orientations). | MEDIUM | Belongs under the parameter-contract umbrella but is geometric, not scalar — easy to under-specify. Recommend promoting to an explicit table-stakes item with its own assertion + a visual/round-trip sanity check. |
| **[GAP] Artifact schema + versioning** | Plan open-question #7. The cache/table needs a versioned schema (single BLER vs curve, path gains, per-CQI). Without an explicit version field, a schema change silently mis-reads old artifacts. | LOW-MEDIUM | A `schema_version` field + load-time check is cheap insurance and a prerequisite for the cache and fail-loud features being trustworthy across the project's own evolution. |

### Differentiators (Why Anyone Would Opt In)

These are the reasons to choose Sionna over the validated, fast analytic default. They align with PROJECT.md Core Value ("site-specific, geometry-derived channel/BLER").

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Authored "some world" scene mode** (synthetic boxes or a real map, e.g. the built-in Munich scene) | This is the headline capability Simu5G *fundamentally lacks*: site-specific propagation from real 3D geometry, materials, reflections. Demonstrates the value proposition concretely. | MEDIUM | Standalone Sionna scene; needs the coordinate transform (GAP above) to align with node positions, but does **not** require deriving geometry from INET buildings. A real-map scene is the most compelling demo for the research community. |
| **Path-gain-based RSRP / received power** | Makes desired-signal, interferer powers, and `getRSRP()` site-specific too — not just BLER. Sionna computes path gains in RT anyway, so it is low marginal cost for high realism. | LOW-MEDIUM | Override `getRSRP()`/`getRSRP_D2D()` (verified pure-virtuals) to return Sionna per-link path gain. Enables coherent site-specific RSRP-driven behavior (handover-like metrics, cell selection inputs) even in v1's noise-limited cut. |
| **Optional auto-invocation from the sim onto the same artifact** | Convenience: the sim can spawn the offline tool to (re)produce the cached artifact if missing, instead of a manual two-step. The clean *default* stays "load a pre-made artifact" (no runtime Python). | MEDIUM | Strictly additive over the offline-tool baseline. Must preserve the "no Python in a normal run" guarantee — auto-invoke is an opt-in convenience path, not the default. Depends on the offline tool + cache existing first. |
| **[GAP] Bounded-difference calibration *report* artifact** | Turning the empty-world calibration from an internal check into a published, human-readable comparison (Sionna vs 3GPP, with the tolerance band) is what makes the module *trustworthy to others*, not just to its author. | LOW-MEDIUM | A differentiator for community adoption: reproducible evidence that the integration is sane. Reuses the calibration-mode machinery; mostly reporting/plotting on top. |

### Anti-Features (Deliberately NOT in v1)

These map onto PROJECT.md "Out of Scope" plus the plan's deferrals. Documented to prevent scope creep; each has the intended later path.

| Feature | Why Requested | Why Problematic (for v1) | Alternative / Later Path |
|---------|---------------|--------------------------|--------------------------|
| **Per-TTI runtime coupling / runtime IPC** | "Live" channel feels more accurate/dynamic. | Reintroduces per-TTI Python/GPU overhead, kills determinism, breaks the no-runtime-dependency promise; the static channel makes it unnecessary. | Precompute-once cached artifact; v1 channel is time-invariant so runtime coupling adds cost, not accuracy. |
| **Embedded Python / pybind11 in the C++ build** | One process, no subprocess plumbing. | Adds Python/TF/GPU to the build, violating the byte-for-byte-unaffected default; couples build to a heavy toolchain. | Offline tool produces an artifact; C++ side only loads data. Subprocess auto-invoke (differentiator) stays out of the default build. |
| **Mobility / dynamic node positions** | Realism; most 5G studies move UEs. | Time-varying channel breaks precompute-once and the single-BLER-per-link cut; needs Doppler/time evolution + re-tracing. | v1 strictly static. Schema *may* retain path-level data (plan §9) to keep a path to Doppler/mobility later. |
| **Dynamic transmit-power control** | Real schedulers adapt power. | Adds a power dimension to the precompute and couples runtime control to the channel; not needed to validate the geometry→BLER pipeline. | Fixed Tx power in v1; power-control dimension deferred. |
| **Dynamic interference curves (BLER-vs-SINR with path gains)** | Multi-cell realism; interference is where system simulators earn their keep. | More precompute (extra table dimension) and not needed to validate the RT→lookup pipeline; v1 is noise-limited and a *strict subset* of v2, so nothing is wasted. | v2: Sionna returns BLER-vs-SINR curves + path gains; Simu5G keeps owning SINR. Same machinery, one extra dimension. |
| **Full per-RB MIMO matrices + rank adaptation** | Highest PHY fidelity; frequency-selective per-RB behavior. | Separate, much larger PHY-abstraction effort; collapses the scalar effective-channel pipeline. | v1 bakes a chosen beamformer into the effective per-link channel (scalar). Per-RB MIMO is its own milestone. |
| **INET-buildings → Sionna scene derivation** | Single-source-of-truth geometry; "both systems know the same thing." | Simu5G consumes **no** building geometry today (verified: only scalar 38.901 penetration loss), so it is *not a v1 correctness requirement*; adds a transform/derivation burden. | Later feature toward full single-source-of-truth. v1 uses standalone authored scenes; positions are the only hard cross-system contract. |
| **Full link-level LDPC Monte Carlo as the v1 BLER method** | Highest BLER fidelity (real PUSCH/PDSCH chain). | A Monte Carlo per (link, MCS) entry → expensive precompute, slow iteration during bring-up. | v1 uses effective-SINR → reference curve. LDPC Monte Carlo is an opt-in higher-fidelity backend later. |
| **[GAP] Per-redundancy-version (per-HARQ-retransmission) BLER precompute** | More accurate HARQ combining than a heuristic. | Multiplies precompute entries and complicates the schema before the base pipeline is validated. | Keep Simu5G's `harqReduction_^(txNumber-1)` heuristic on top of base Sionna BLER in v1 (plan open-question #5). Promote to precomputed per-RV only if the heuristic proves inadequate. |
| **[GAP] MMSE-IRC / spatially-aware interference rejection** | Captures receivers that exploit interferer spatial structure. | Fundamentally incompatible with any precompute-once scheme (interference treated as white at lookup time). | Document the limitation (plan §5): interference moves the SINR but does not reshape the curve. Interference-free links are exact. |

## Feature Dependencies

```
Opt-in SionnaChannelModel (NED/ini seam, build isolation)
    └──requires──> Division of labor (Sionna owns BLER, Simu5G owns SINR)
                       └──requires──> Effective-SINR → reference-curve method
                                          └──requires──> Artifact schema + versioning [GAP]
                                                             └──requires──> Precompute-once + request-hash cache

Parameter-contract assertion (fail-loud)
    └──requires──> Shared scenario source (positions/antennas/carrier from one description)
    └──requires──> Coordinate/units transform [GAP]   (geometric half of the contract)

Precompute-once + cache
    └──requires──> Coordinate/units transform [GAP]    (positions must land in the scene correctly)
    └──enables──>  Own fingerprint baselines (need a pinned artifact to baseline against)
    └──enables──>  Optional auto-invocation (differentiator)

CQI-feedback consistency
    └──requires──> Artifact schema holds BLER for ALL CQIs/MCS per link
    └──requires──> Division of labor (same table feeds scheduler and realized BLER)

No double-counting (Sionna owns path gain)
    └──requires──> Path-gain extraction from Sionna   (also powers path-gain RSRP differentiator)
    └──conflicts──> Simu5G statistical shadowing / random LOS / 38.901 penetration (must be disabled when active)

Reference / empty-world calibration mode
    └──requires──> Parameter-contract assertion (calibration is the test of the contract)
    └──requires──> Authored-scene machinery (empty world is the degenerate scene)
    └──enables──>  Bounded-difference calibration report [GAP] (differentiator)

Authored "some world" scene (differentiator)
    └──requires──> Coordinate/units transform [GAP]
    └──does NOT require──> INET-buildings derivation (anti-feature)
```

### Dependency Notes

- **Everything trustworthy hangs off the parameter contract + coordinate transform.** These two (one scalar, one geometric) are the silent-corruption guards. The transform is currently implicit in PROJECT.md and should be made explicit — it is the single most likely source of plausible-but-wrong results.
- **Calibration mode is the empty-scene special case of the authored-scene machinery**, so building scene loading + the transform first gives calibration almost for free. Order: transform → scene loading → empty-world calibration → authored "some world."
- **CQI consistency forces the schema to be per-CQI/MCS** (all entries, not just the selected one). This is a schema decision (plan #7) that must be made *before* the precompute is written, or the table has to be regenerated.
- **Fingerprint baselines depend on a pinned cache**, which depends on a stable schema and a complete request hash. Baselines are last in the chain, not a standalone early task.
- **Path-gain extraction is shared infrastructure**: it satisfies the no-double-counting table-stakes requirement *and* powers the path-gain-RSRP differentiator. Build once.
- **v1 is a strict subset of v2 by construction** (noise-limited → single BLER is the degenerate case of BLER-vs-SINR curves). This is a deliberate dependency *non*-trap: nothing in v1 is thrown away when interference curves arrive.

## MVP Definition

### Launch With (v1) — all table stakes

- [ ] Opt-in `SionnaChannelModel` (NED/ini seam + build isolation) — the core promise; nothing ships without it
- [ ] Division of labor + effective-SINR→reference-curve BLER (noise-limited, one BLER per link/MCS) — the pipeline being validated
- [ ] Artifact schema + versioning **[GAP]** and precompute-once request-hash cache — reproducibility + cheap reruns
- [ ] Coordinate/units transform **[GAP]** + shared scenario source — geometric correctness
- [ ] Parameter-contract assertion, fail-loud — trust guard
- [ ] No double-counting (Sionna owns path gain; disable Simu5G shadowing/LOS/penetration) — physical correctness
- [ ] CQI-feedback consistency against the same table — internal coherence
- [ ] Reference / empty-world calibration mode — the validation anchor
- [ ] Own fingerprint baselines (pinned cache) — green CI / reproducibility

### Add After Validation (v1.x) — differentiators

- [ ] Authored "some world" real-map scene (e.g. Munich) — trigger: empty-world calibration passes its tolerance band
- [ ] Path-gain-based RSRP — trigger: path-gain extraction validated in calibration
- [ ] Bounded-difference calibration report artifact **[GAP]** — trigger: calibration band agreed, ready to publish evidence
- [ ] Optional auto-invocation onto the cached artifact — trigger: offline tool + cache stable and trusted

### Future Consideration (v2+) — formerly anti-features

- [ ] Dynamic interference curves (BLER-vs-SINR + path gains) — defer: v1 noise-limited subset must be validated first
- [ ] Full per-RB MIMO + rank adaptation — defer: separate PHY-abstraction milestone
- [ ] Mobility / Doppler / time evolution — defer: requires breaking time-invariance; keep schema hooks (plan §9)
- [ ] INET-buildings → Sionna scene derivation — defer: not a correctness need today; single-source-of-truth goal
- [ ] Full link-level LDPC Monte Carlo backend — defer: higher fidelity, costlier precompute
- [ ] Dynamic transmit-power control — defer: adds a precompute dimension
- [ ] Per-redundancy-version BLER **[GAP]** — defer: only if the HARQ heuristic proves inadequate

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Opt-in seam + build isolation | HIGH | MEDIUM | P1 |
| Parameter-contract assertion (fail-loud) | HIGH | MEDIUM | P1 |
| Coordinate/units transform **[GAP]** | HIGH | MEDIUM | P1 |
| Artifact schema + versioning **[GAP]** | HIGH | LOW-MEDIUM | P1 |
| Precompute-once + request-hash cache | HIGH | MEDIUM | P1 |
| Division of labor + effective-SINR→curve | HIGH | MEDIUM-HIGH | P1 |
| No double-counting (path-gain ownership) | HIGH | MEDIUM | P1 |
| CQI-feedback consistency | HIGH | MEDIUM | P1 |
| Reference / empty-world calibration mode | HIGH | MEDIUM-HIGH | P1 |
| Own fingerprint baselines | MEDIUM | LOW-MEDIUM | P1 |
| Authored "some world" scene | HIGH | MEDIUM | P2 |
| Path-gain-based RSRP | MEDIUM | LOW-MEDIUM | P2 |
| Calibration report artifact **[GAP]** | MEDIUM | LOW-MEDIUM | P2 |
| Optional auto-invocation | MEDIUM | MEDIUM | P2 |
| Dynamic interference curves | HIGH | HIGH | P3 |
| Per-RB MIMO + rank adaptation | MEDIUM | HIGH | P3 |
| Mobility / Doppler | HIGH | HIGH | P3 |
| INET-buildings derivation | LOW (today) | HIGH | P3 |
| LDPC Monte Carlo backend | MEDIUM | HIGH | P3 |

**Priority key:** P1 = must have for v1 launch · P2 = differentiator, add after calibration passes · P3 = v2+ / deferred anti-feature.

## Competitor Feature Analysis

The "competitor" is Simu5G's own analytic default; the comparison is the opt-in tradeoff users face.

| Feature | Simu5G analytic default | Generic offline BLER tables (PhyPisaData) | Our Sionna approach |
|---------|-------------------------|-------------------------------------------|---------------------|
| Site-specific geometry | No (distance + freq + random LOS) | No (AWGN/TU `[3][15][49]`) | **Yes** (RT over real 3D scene) — the differentiator |
| Runtime cost | Very low (analytic C++) | Very low (table lookup) | Low at runtime (precompute-once cached lookup); high one-time precompute |
| Determinism / fingerprints | Established baselines | Established baselines | New, pinned-artifact baselines (own set) |
| Interference handling | Dynamic SINR aggregation | Dynamic SINR aggregation | Kept (Simu5G owns SINR); v1 noise-limited, v2 curves |
| Build dependencies | None | None | None in default build; Python/TF/GPU only in offline tool |
| Validation story | Community-validated | Community-validated | Empty-world calibration vs 3GPP, bounded difference |

## Sources

- `/home/zoli/Projects/OMNET/Simu5G/.planning/PROJECT.md` — Active / Out-of-Scope / Key Decisions (CURATED, HIGH)
- `/home/zoli/Projects/OMNET/Sionna/sionna-integration-plan.md` — design draft, §§3–9, code anchors (CURATED, HIGH)
- `/home/zoli/Projects/OMNET/Sionna/sionna.elony.hatrany.md` — pros/cons, runtime-vs-offline tradeoff (CURATED, MEDIUM)
- Verified Simu5G source: `ILteChannelModel.ned` (polymorphic seam), `LteChannelModel.h` (pure-virtual `getSINR`/`getRSRP`/`isReceptionSuccessful`/`getAttenuation`, `getNumBands`/`getCarrierFrequency`/`getNumerologyIndex` contract surface), `PhyPisaData.h` (`getBler(txMode,cqi,sinr)`, `[3][15][49]` table), `LteFeedbackComputationRealistic.h` (`getCqi`, `targetBler_`) (HIGH)
- Standard link-to-system / effective-SINR-mapping methodology and Sionna RT capabilities (background knowledge, stable; HIGH for table stakes, MEDIUM for fidelity-method risk)

---
*Feature research for: opt-in ray-tracing-derived channel module (Simu5G × Sionna RT)*
*Researched: 2026-06-17*
