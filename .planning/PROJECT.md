# Simu5G × Sionna Integration

## What This Is

An **optional, opt-in** channel-model module that lets Simu5G replace its statistical
PHY/link abstraction with results derived from **NVIDIA Sionna RT** (ray tracing over real
3D geometry), giving site-specific, physically grounded propagation and BLER. It is built
as a reusable Simu5G capability for the research community: the default Simu5G build and
behavior stay completely unaffected, and no Python/TensorFlow/GPU dependency is added to a
normal build or run. Initial scope targets **completely static scenarios** with
precompute-once channel data.

## Core Value

Simu5G can opt in to site-specific, geometry-derived channel/BLER from Sionna **without
changing the default build or behavior** — the analytic model remains the untouched default,
and Sionna is a clean, selectable alternative.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

(None yet — ship to validate)

### Active

<!-- Current scope (v1). All are hypotheses until shipped and validated. -->

- [ ] Opt-in `SionnaChannelModel : LteChannelModel`, selectable via NED polymorphism
  (`like ILteChannelModel`) + an ini setting; default build and behavior unaffected; no
  Python/TF/GPU in a normal build or run.
- [ ] Division of labor: Sionna owns the **SINR→BLER mapping**; Simu5G owns the **SINR value**
  (interference + noise aggregation retained). v1 is noise-limited → one BLER per (link, MCS).
- [ ] Precompute-once + **cache keyed by a hash of the full request** (scene, positions,
  materials, antennas, freqs, powers, MCS set); reruns of the same scenario skip Sionna.
- [ ] Invocation: **dual-source** (PA-2) — default loads the offline-produced cached artifact;
  opt-in `channelSource=subprocess|auto` lets `SionnaManager` spawn an **external** Sionna
  subprocess (never embedded Python). Source strategy is pluggable in the manager (PA-3); the
  subprocess is the seam toward quasi-static dynamics (PA-6).
- [ ] Internal BLER method: **effective-SINR → reference curve** (RT channel → post-eq.
  effective SINR → BLER from reference curves).
- [ ] **Reference / empty-world calibration mode**: an obstacle-free Sionna world,
  parameterizable to approximate Simu5G's modeled conditions, used as the comparability and
  parameter-contract validation anchor (catch silent unit/convention mismatches).
- [ ] **Authored "some world" scene mode**: a standalone Sionna scene (synthetic boxes or a
  real map such as the built-in Munich scene) demonstrating the site-specific capability.
- [ ] **Shared scenario source** drives both sides: node positions, antenna heights/orientation,
  carrier frequency, numerology, bandwidth, band count come from one description; the model
  asserts the loaded dataset matches and **fails loudly** on mismatch.
- [ ] **No double counting**: when `SionnaChannelModel` is active it fully owns path gain;
  Simu5G's statistical shadowing / random LOS draw / penetration loss are not applied on top.
- [ ] CQI feedback path uses the **same** Sionna table (highest MCS with BLER ≤ target), so
  scheduler MCS and realized BLER agree; the table holds BLER for all CQIs/MCS per link.
- [ ] Sionna configs get their **own fingerprint baselines** (RNG-stream consumption differs
  from the statistical model); cached table pinned for reproducibility.

### Out of Scope

<!-- Explicit boundaries with reasoning. -->

- Mobility / dynamic node positions — v1 is completely static (time-invariant channel); design
  keeps a path to it but does not build it.
- Per-TTI / runtime coupling / embedded Python / runtime IPC — precompute-once only; avoids
  runtime Python/GPU overhead and keeps determinism.
- Dynamic transmit-power control — fixed Tx power in v1.
- Dynamic interference curves (BLER-vs-SINR with path gains) — deferred to v2; v1 is
  noise-limited. v1 is a strict subset, so nothing is thrown away.
- INET-buildings → Sionna scene derivation — later feature toward full single-source-of-truth;
  not a v1 correctness requirement since Simu5G consumes no building geometry today.
- Full per-RB MIMO matrices + rank adaptation — separate, larger PHY-abstraction effort; v1
  bakes a chosen beamformer into the effective per-link channel.
- Full link-level LDPC Monte Carlo as the v1 BLER method — higher fidelity but costlier;
  effective-SINR → reference curve is the v1 choice.

## Context

- **Brownfield.** Extends Simu5G (`/home/zoli/Projects/OMNET/Simu5G`, branch `v1.4.5`), an
  OMNeT++/INET-based system-level 5G (NR) simulator written in C++. Sionna RT 2.0.1 +
  sionna_rt 2.0.1 are installed in a sibling venv (`/home/zoli/Projects/OMNET/Sionna`), with
  exploratory RT scripts (`rt_step1.py`, `rt_step2.py`) and rendered scenes.
- **Confirmed baseline behavior (verified in source):** Simu5G does **not** use specific 3D
  object geometry in its channel/BLER computation. Path loss = distance + carrier freq +
  LOS flag (analytic 3GPP formulas); LOS/NLOS = random draw vs a distance-based probability;
  shadowing = Gaussian random process; BLER = `PhyPisaData::getBler(txMode, cqi, snr)` table
  lookup over a `[3][15][49]` AWGN/TU table. The only building effect is the NR 38.901
  penetration loss, which is scalar/statistical, not geometric.
  - Consequence: v1's only hard cross-system consistency requirement is **node positions**
    (+ antennas, carrier/numerology). Sionna buildings can be a standalone "some world" and
    already give a capability Simu5G fundamentally lacks.
- **Design source of truth:** `Sionna/sionna-integration-plan.md` (detailed design draft) and
  `Sionna/sionna.elony.hatrany.md` (pros/cons). This PROJECT.md captures the decisions agreed
  during questioning; the plan's remaining open items move to the planning phase.

## Constraints

- **Tech stack**: Simu5G / OMNeT++ / INET, C++. Sionna RT is Python/TensorFlow/GPU — must stay
  outside the normal build; coupling is via a precomputed, cached data artifact.
- **Compatibility**: default Simu5G build, behavior, and existing channel models must remain
  byte-for-byte unaffected; Sionna is strictly opt-in.
- **Determinism / fingerprints**: runs must be reproducible from the pinned cached table;
  Sionna configs need their own fingerprint baselines.
- **Parameter contract**: carrier frequency, bandwidth, numerology/SCS, band count, antenna
  arrays/patterns, Tx-power convention, polarization, and SINR x-axis definition must be
  identical on both sides; mismatch must fail loudly at init.
- **Validation honesty**: exact numerical agreement between Sionna (empty world) and Simu5G's
  3GPP statistical formulas is not expected; the target is a bounded, explainable difference.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Opt-in `SionnaChannelModel : LteChannelModel` via NED `like ILteChannelModel` + ini | Zero impact on default build/behavior; clean selectable alternative | — Pending |
| Sionna owns SINR→BLER mapping; Simu5G owns SINR value | Splits static geometry physics (Sionna) from runtime interference/noise (Simu5G's strength) | — Pending |
| v1 noise-limited (one BLER per link/MCS); v2 adds interference curves | v1 is a strict subset of v2 → isolates the geometry/RT/lookup pipeline for validation, nothing wasted | — Pending |
| Precompute-once + cache by request hash | Makes "invoke once" cheap across parameter studies / fingerprint reruns; keeps determinism | — Pending |
| Invocation: separate offline tool + optional sim auto-invoke later | Clean boundaries; C++ side has no runtime Python dependency | — Pending |
| BLER method: effective-SINR → reference curve | Cheaper precompute, faster iteration than full LDPC Monte Carlo | — Pending |
| Mandatory reference/empty-world calibration mode | Comparability anchor + catches silent parameter/unit mismatch before trusting site-specific results | — Pending |
| Shared scenario source (positions/antennas/carrier) drives both; assert + fail loudly | "Both systems know the same thing" north-star; silent geometry/param mismatch corrupts everything | — Pending |
| When Sionna active, it fully owns path gain (no Simu5G shadowing/LOS/penetration on top) | Avoid double-counting deterministic RT gain with random statistics | — Pending |
| **(PA-1)** Pivot to the **Plan A (channel) track**; the BLER/CQI track (Plan B) is parked to a later phase | Mature the site-specific channel + validation harness first; BLER shares the same `SionnaTable` and benefits from the channel decisions landing first | Decided 2026-06-18 |
| **(PA-2)** **Dual-source invocation**: default = load offline artifact; opt-in = sim-spawned **external** Sionna subprocess; `channelSource = artifact\|subprocess\|auto` | Keeps both a pre-generated world and a sim-driven path; the subprocess is the seam toward future dynamics. External process (not embedded) → binary links no Python, so SEAM-02 + "no Python in a normal run" hold (default loads artifact, fail-loud if missing) | Decided 2026-06-18 |
| **(PA-3)** One `SionnaChannelModel` (thin reader) + pluggable **source strategy in `SionnaManager`** (`StaticArtifact\|Subprocess\|(v2)LiveSionna`); no `DynamicSionnaChannelModel` yet | The reception/SINR/RSRP logic is identical static-or-dynamic; only the table's update policy differs → that belongs in the manager. A separate model subclass only if per-call time-dependent math (Doppler) is needed (v2) | Decided 2026-06-18 |
| **(PA-4)** Channel table/exchange format = **versioned JSON with round-trip-exact float repr** (`%.17g`/hex) | Small channel table + dynamic request/response unify cleanly in one human-readable, diffable format both Python paths emit; exact-float repr removes the precision objection. **Amends** CLAUDE.md's "no JSON for the bulk table" for the (small) channel; bulk BLER stays binary/HDF5 if/when it returns | Decided 2026-06-18 |
| **(PA-5)** Adopt a **`CompareChannelModel`** decorator (built-in vs Sionna on identical inputs, RNG-neutral, per-link deltas) | A live, in-sim, apples-to-apples per-link validation two separate runs cannot give; default primary = built-in keeps fingerprints unperturbed | Decided 2026-06-18 |
| **(PA-6)** Future dynamics = **quasi-static, event-driven** regeneration (re-spawn → later long-lived process + IPC); never per-TTI / embedded | Prepares for position changes without violating the no-runtime-Python constraint; per-TTI coupling stays out of scope | Decided 2026-06-18 |
| **(PA-7)** Introduce `interferenceMode` (noise-limited\|all-pairs) + `granularity` (per-RB\|wideband) params + coupling-guard | Forward design toward multi-cell; default stays noise-limited + per-RB; guard rejects the inconsistent wideband+all-pairs combo | Decided 2026-06-18 |
| **(PA-8)** Scene path: **free-space now → flat-ground/two-ray later**; CAL-01 free-space anchor kept as a separate sanity check | Two-ray is more realistic pre-buildings but shifts the calibration reference off Friis; stage it deliberately in the scene phase | Decided 2026-06-18 |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-18 — Plan A (channel) pivot: dual-source invocation, JSON channel format, source-strategy architecture, CompareChannelModel; BLER track parked to Phase 5*
