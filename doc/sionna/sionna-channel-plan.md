# Plan A — Sionna Channel (ray-traced attenuation)

Status: design discussion / draft
Parent: [Sionna PHY Integration — Overview](sionna-integration-plan.md)
Sibling: [Plan B — Sionna BLER curves](sionna-blercurves-plan.md)

Scope: replace Simu5G's analytic propagation with **site-specific, ray-traced
attenuation** from NVIDIA Sionna RT, for **completely static scenarios**. This plan
covers *only* the channel (Component A); the `SINR → BLER` mapping is Plan B.

---

## 1. Goal & boundary

Sionna RT computes the **attenuation / path gain** for every Tx–Rx pair (desired
link **and** every interferer pair), from real 3D geometry. This replaces Simu5G's
statistical path-loss / shadowing / fading models with physically grounded,
site-specific values.

Boundary: this plan **feeds** Simu5G's SINR aggregation; it does **not** decide
block error (that is Plan B, via `ICurveProvider`). The handoff is the per-RB SINR
(see §6).

## 2. Scope & assumptions (v1)

- **Static scenarios**: node positions fixed; channel time-invariant.
- **Offline / precompute-once**: the simulation invokes Sionna RT once at startup
  (or loads a cached artifact) and reuses the channel for the whole run. No per-TTI
  coupling, no embedded Python, no runtime IPC.
- **Opt-in & isolated**: a new channel-model module; existing models untouched.
- **Tx power need not be fixed**: attenuation is a power-independent multiplier;
  Simu5G applies runtime Tx power when forming SINR. Power control works without
  re-running Sionna.

## 3. What Sionna provides, and the link budget

- Per-(link, RB) **attenuation** (a power-independent multiplier), for all relevant
  Tx–Rx pairs. Plus per-link path gain → `getRSRP()`.
- **Reference points / no double-counting.** Sionna's attenuation spans Tx antenna
  port → Rx antenna port (propagation + antenna patterns + beamforming). Therefore
  Simu5G must **not** re-apply its own antenna gains; it adds only receiver-side
  terms — thermal noise, noise figure, (cable loss). Do not apply any fading term
  on top: the ray-traced channel already encodes the fades.
- **MIMO / beamforming (v1)**: bake a fixed beamformer into the per-link
  attenuation (keeps Simu5G's scalar-per-RB pipeline). True per-RB MIMO matrices +
  rank adaptation is a later effort (and re-introduces geometry into Plan B's
  curves — see Plan B).
- **Granularity**: per-RB attenuation (matches Simu5G's per-band `getSINR()` vector)
  vs. one wideband value — **user-selectable** (§8). Per-RB is required for
  RB-selective interference (see the coupling guard in §8).

## 4. Architecture

### 4.1 Global channel precompute, thin per-PHY lookup

- **Decided: a dedicated `SionnaManager` module** (not the `Binder`) owns the
  precompute. It runs **one RT pass for the whole network**, producing the
  attenuation table for all needed Tx/Rx pairs in a single Sionna invocation, and
  queries the `Binder` (the global node/cell registry) to enumerate nodes and
  positions.
- `SionnaManager` parameters include an optional **`sceneFile`** (a Sionna-native
  scene; see §5), the channel-table cache path, and the **user-selectable**
  `interferenceMode` (noise-limited vs. all-pairs) and `granularity` (per-RB vs.
  wideband) settings (§8).
- Per-PHY `SionnaChannelModel : public LteChannelModel` instances are **thin
  lookups** into the shared table held by `SionnaManager`.

### 4.2 Code seams (Simu5G side)

- New `SionnaChannelModel : public LteChannelModel`
  (`src/simu5g/stack/phy/channelmodel/`), selectable via `ILteChannelModel` /
  `LtePhyBase`'s `channelModelModule`.
- `getSINR()` — keep the interference + noise **aggregation skeleton**, but source
  every received power (desired + interferers) from the Sionna attenuation table.
  The analytic machinery is bypassed: `getAttenuation()`, `computePathLoss()`,
  `computeShadowing()`, `jakesFading()` / `rayleighFading()`,
  `computeAngularAttenuation()`.
- `getRSRP()` / `getRSRP_D2D()` — direct read of the Sionna desired-link path gain.
- `isReceptionSuccessful()` / `_D2D` / `_bgUe` — unchanged in v1 except that the
  per-RB SINR comes from the Sionna-fed `getSINR()`; the BLER lookup is Plan B.

### 4.3 Channel table, invocation & caching

- At the init stage where positions are final, `SionnaManager` enumerates the
  needed (txPos, rxPos, antenna, carrier) pairs — **serving links only** under
  noise-limited mode, or **all Tx→Rx pairs** under all-pairs mode — spawns the
  Sionna RT subprocess; reads back the attenuation table. The JSON schema is
  all-pairs-capable regardless of mode, so switching modes is config, not a schema
  change.
- **Decided: the channel table is JSON** — versioned, with the per-RB-vs-wideband
  flag and the carrier/numerology contract recorded in the header. Human-readable
  and diffable; size is modest for static scenarios. (Revisit only if a very large
  scenario makes JSON impractical.)
- **Cache by a hash of the request** (scene, positions, materials, antennas,
  freqs). Rerun with the same scenario → skip Sionna, load cache → fingerprint
  stable.
- **Decided: invocation = sim-orchestrated subprocess.** `SionnaManager` spawns the
  Sionna RT process at startup (a single spawn); the cached artifact still lets
  reruns skip it. (A fully separate offline tool is not needed.)

### 4.4 Determinism

- The channel table is a frozen artifact; pin/commit it for stable fingerprints.
- Sionna configs get their own baselines (no per-link fading RNG draws).

---

## 5. Scene sourcing (deferred)

**Decision: flat ground in the 1st stage.** The first stage runs with a **flat
ground plane only** (no buildings/obstacles) — a direct path plus a ground-reflected
path (two-ray-like), not pure free space. `SionnaManager` exposes an **optional
`sceneFile` parameter** pointing to a **Sionna-native scene** (Mitsuba 3 XML — the
format Sionna RT consumes directly) as a hook for adding real geometry later, but no
Simu5G/INET geometry integration is done now. This keeps the geometry concern
entirely on the Sionna side and avoids the coordinate-frame coupling work.

The flat ground needs a ground material (permittivity/conductivity) and ground
reflections enabled in Sionna RT (≥1 reflection). Building diffraction/scattering
stay off until the scene phase.

Caveat to record: with an external `sceneFile`, the scene has its **own coordinate
frame**, so node positions from OMNeT++ must be expressed in that frame. For v1 this
is the user's responsibility (document the convention); a validated transform +
alignment check is future work if/when geometry integration is taken up.

### Parked options (for a later geometry phase)

Researched but not adopted now:

- **(1) INET `PhysicalEnvironment` as single source of truth** — INET (at
  `/home/andras/projects/inet-mipv6`) has a dormant obstacle model (XML
  cuboid/prism/polyhedron/sphere + `Material` with εr/μr/resistivity) that Simu5G
  ignores. It shares OMNeT++ `Coord` with node positions ⇒ **one** coordinate
  frame, no transform. Materials map cleanly to Sionna `RadioMaterial`
  (εr→εr, σ=1/resistivity, μr≈1); primitives triangulate trivially. The most
  OMNeT++-native path; natural choice when geometry integration is revisited.
- **(2) USD / Omniverse / AODT** — richest geometry, but **USD is not native to
  Sionna RT** (USD belongs to NVIDIA's separate Aerial Omniverse Digital Twin);
  needs USD → glTF/OBJ → Mitsuba conversion and a coordinate transform.
- **(3) Sionna-native Blender/OSM** — what the `sceneFile` path uses; best-supported
  by Sionna; still a separate coordinate frame from OMNeT++.

Also deferred to that phase: real terrain (beyond the stage-1 flat ground) and
building-level Sionna RT interactions (diffraction, scattering) — though the stage-1
flat ground already exercises ground reflection.

---

## 6. Interface to Plan B / Simu5G

- Plan A's output → Simu5G forms per-RB SINR (§3 link budget) → handed to
  `ICurveProvider` (Plan B).
- **Consistency rule**: the per-RB SINR definition must match the curve x-axis. v1 =
  per-RB SINR + AWGN curve (selectivity is in the per-RB SINR spread). See
  [Plan B](sionna-blercurves-plan.md).

---

## 7. Validation (compare against the built-in model)

A decorator channel model runs the built-in 3GPP model and the Sionna model on
**identical inputs** (same positions, Tx power, RB allocation from one scheduling
timeline) and records their differences — a true per-link, apples-to-apples
comparison that two separate runs cannot give (those diverge via scheduling).

### Structure

- `CompareChannelModel : public LteChannelModel` — must inherit `LteChannelModel`
  because `LtePhyBase` does `check_and_cast<LteChannelModel*>` on its
  `channelModelModule` (a compound NED wrapper would fail that cast).
- It references two sibling channel-model submodules by parameter path —
  `referenceModule` (built-in, e.g. `NrChannelModel_3GPP38_901`) and
  `candidateModule` (`SionnaChannelModel`) — the same pattern as `binderModule` /
  `cellInfoModule`. No dynamic module creation; each inner model keeps its own
  parameters.
- A `primary` parameter selects whose value is returned to the simulation. Each
  overridden method forwards identical arguments to both, emits the deltas, and
  returns the primary's result:
  ```
  getSINR(frame, lteInfo):
      ref  = reference_->getSINR(frame, lteInfo)
      cand = candidate_->getSINR(frame, lteInfo)
      emitDeltas(ref, cand, lteInfo)        // → signals → vec/sca
      return primaryIsReference_ ? ref : cand
  ```

### RNG neutrality (critical) — default primary = built-in

The built-in model draws random numbers (log-normal shadowing, Jakes/Rayleigh
fading) and mutates state; Sionna is deterministic and draws nothing. So make the
**deterministic** model the comparison-only side: **default `primary` = built-in,
`candidate` = Sionna.** Then the comparison calls into Sionna draw no random numbers
and the run behaves *exactly* like a normal built-in run — same fingerprints — with
Sionna deltas logged on top. Choosing `primary` = Sionna instead requires giving the
built-in its **own dedicated RNG stream** (OMNeT++ per-module `rng-N` mapping) so its
comparison draws do not perturb the shared streams.

### What is compared

- **Attenuation / path gain** (`getAttenuation` / `getRSRP`) — pure propagation, no
  interference; the cleanest, most direct comparison of what Plan A changes.
- **Per-RB SINR** (`getSINR`) — propagation **plus** interference (each model uses
  its own interference internally, so the SINR delta also reflects interference
  differences; read it together with the attenuation delta).
- **Reception** — compare the **BLER probability** (deterministic from SINR+CQI),
  not the random success boolean; only the primary performs the actual draw.

### Fairness mode

Per-call deltas are noisy because the built-in includes random fast fading while
Sionna (static) does not. Two modes:
- **Full**: compare as-is; interpret deltas as a distribution (bias + RMSE).
- **Large-scale only**: disable the built-in's fading so it is path-loss + shadowing
  vs. Sionna's mean — a fair point comparison of the large-scale channel.

### Outputs (vec/sca)

Per-link/per-band **vectors**: `attenuationRef`, `attenuationSionna`,
`attenuationDelta` (dB); same for RSRP and SINR — tagged by node/link/band.
End-of-run **scalars**: mean delta (bias), std, RMSE, correlation, percentiles.

### Scope & cross-check

- This validates **Plan A** (channel). **Plan B** (BLER curves) is validated
  separately — a standalone offline diff of Sionna-generated curves vs `PhyPisaData`.
- Keep "two full runs + offline vec/sca diff" as an occasional aggregate cross-check
  (simpler, no RNG entanglement, but only aggregate-comparable since runs diverge).

---

## 8. Decisions & open questions (channel-specific)

**Decided:**
- **Precompute owner** — a dedicated `SionnaManager` module (not `Binder`).
- **Channel table format** — JSON (versioned), all-pairs-capable schema.
- **Scene sourcing** — 1st stage uses a **flat ground plane** (no buildings);
  optional `sceneFile` hook on `SionnaManager` for real geometry later (§5).
- **Invocation model** — sim-orchestrated subprocess spawned by `SionnaManager`.
- **Interference** — **user-selectable** via `SionnaManager.interferenceMode`:
  *noise-limited* (serving links only, SINR = SNR) vs. *all-pairs* (full Tx→Rx
  matrix, active interferers summed per TTI).
- **Granularity** — **user-selectable** via `SionnaManager.granularity`: *per-RB*
  (per-band vector, matches Simu5G's existing reception) vs. *wideband* (one value
  per link).
- **MIMO / beamforming** — bake a fixed beamformer into the per-link attenuation for
  v1 (keeps the scalar-per-RB pipeline); full per-RB MIMO + rank adaptation later.
- **Temporal/Doppler** — out of scope for v1 (time-invariant); keep path-level data
  in the schema to enable Doppler / mobility / live coupling later.
- **Validation** — a `CompareChannelModel` decorator runs the built-in and Sionna
  models on identical inputs and records per-link deltas (§7).

**Coupling guard:** *wideband* + *all-pairs* is not fully consistent — a single
wideband value cannot represent RB-selective interference, so interference would be
treated as band-averaged (an approximation). `SionnaManager` should warn (or reject)
this combination. *per-RB* + *all-pairs* is the accurate multi-cell setting;
*wideband* pairs cleanly only with *noise-limited*.

No stage-1 modeling items remain open; the rest is implementation.

---

## 9. Relevant code (anchors)

Simu5G:
- `src/simu5g/stack/phy/channelmodel/LteChannelModel.h` — base class; `getSINR()`,
  `getRSRP()`, `getAttenuation()`.
- `src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.{h,cc}` — the
  propagation methods replaced by Sionna; interference routines whose **aggregation
  structure is kept** (`computeDownlinkInterference()` / `computeUplinkInterference()`).
- `src/simu5g/stack/phy/channelmodel/ILteChannelModel.ned`,
  `src/simu5g/stack/phy/LtePhyBase.{h,cc,ned}` — module selection / per-carrier.
- `src/simu5g/common/carrierAggregation/ComponentCarrier.h` — carrier freq,
  numerology.
- `Binder` — global node/cell registry that `SionnaManager` queries to enumerate
  nodes/positions (no longer the precompute owner).

INET (`/home/andras/projects/inet-mipv6`, for the parked geometry phase only):
- `src/inet/environment/common/PhysicalEnvironment.{ned,h,cc}`,
  `PhysicalObject.{h,cc}`, `Material.{h,cc}` — obstacle geometry + EM materials.
- `src/inet/common/geometry/shape/{Cuboid,Sphere,Prism,polyhedron/Polyhedron}.h`,
  `src/inet/common/geometry/common/Coord.h` — shapes + shared coordinate system.

---

## 10. References (Sionna scene format)

- Introduction to Sionna RT — https://nvlabs.github.io/sionna/rt/tutorials/Introduction.html
- `sionna.rt.scene` API — https://nvlabs.github.io/sionna/_modules/sionna/rt/scene.html
- Custom scene via XML (no Blender) — https://github.com/NVlabs/sionna/discussions/942
- Sionna RT paper (arXiv 2303.11103) — https://arxiv.org/pdf/2303.11103
- AODT Scene Importer (USD / CityGML / OSM) — https://docs.nvidia.com/aerial/aerial-dt/text/scene_importer.html
