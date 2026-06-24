# Code review — `topic/bz/sionna-integration` (Sionna Plan A)

High-effort review (8 finder angles + verification) over the Sionna work diff
(`1c5e55ff..HEAD`), excluding the environmental `simulations.csv` mass re-record and
the docs. Candidates were deduped and verified; the 10 real findings below are ranked
by severity.

> Note: the `rbmap[MACRO][i]` "divergence" was **refuted** — the base also uses
> `operator[]` ([LteRealisticChannelModel.cc:652](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L652)),
> so it is not a regression.

## Correctness

### 1. [medium-high] Background/external-cell interference loses antenna gain, cable loss and angular attenuation
[SionnaChannelModel.cc](../../src/simu5g/stack/phy/channelmodel/SionnaChannelModel.cc) —
`initialize` globally zeroes `antennaGainEnB_`/`cableLoss_`, and the
`computeAngularAttenuation`→0 / `computeShadowing`→0 overrides are global too. For
desired/in-scene links this is correct (the Sionna path gain already includes them). BUT
`computeBackgroundCellInterference` uses **analytic** propagation (background cells are not
in the scene) and also calls `computeAngularAttenuation` and adds `antennaGainEnB_`. So the
background/ext interference **loses** ~18 dBi eNB gain + angular + cable → under-estimated
interference in any scenario with background cells (e.g. the MEC networks with `numBgCells`).
*Altitude:* the zeroing is too broad — the Sionna-specific terms should apply only to
in-scene links.

### 2. [medium] `sionna_rt.py`: coincident Tx/Rx (d=0) → division by zero → `inf` → invalid JSON
[sionna_rt.py](../../src/simu5g/stack/phy/channelmodel/sionna/sionna_rt.py)
`_two_ray_path_gain_db` — the `1e-3` floor protects only `ht`/`hr`, not `d_los`/`d_ref`. Two
distinct nodes at the same position → `d_los=0` → `exp(...)/d_los` = `inf` → `power_gain=inf`
→ `json.dump` (default `allow_nan=True`) writes the **`Infinity`** token, which strict JSON
(and the C++ `nlohmann` reader) reject → the table fails to load. Fix: map non-finite values
to the `-300.0` sentinel and/or pass `allow_nan=False`.

### 3. [medium] `sionna_rt.py`: the two backends place per-RB frequencies differently (even numBands)
The tworay `band_center_frequencies` is symmetric `(i-(n-1)/2)*bw`; the sionna backend uses
`subcarrier_frequencies(n,scs)` = `(arange(n)-n//2)*scs` (asymmetric for even n). For an even
band count (the schema example uses 6) the band-index→frequency mapping **differs** between
the backends; since `auto` switches silently, the committed tworay artifact and a live sionna
run are misaligned per band. (Practical impact in v1 is small because the path gain varies
slowly across bands, but it is a real inconsistency.)

### 4. [medium, latent] `SionnaManager`: `posToId_` position collision
[SionnaManager.cc](../../src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc) —
`posToId_[posKey(pos)] = id` with no collision check. Two **distinct** nodes rounded to the
same mm position (e.g. a co-located gNB+UE) overwrite each other → one coordinate resolves to
the wrong id → wrong link / nullptr. (The NR dual-id UE happens to work because both ids have
links; genuinely distinct co-located nodes break.)

### 5. [medium, latent] Triple carrier-registration crash if the decorator/Sionna model sits at an eNB
`CompareChannelModel` (and `SionnaChannelModel`) inherits `LteChannelModel::initialize`, which
calls `cellInfo_->registerCarrier(...)`; the decorator's two inner models (ref+cand) register
the **same** carrier on the same cellInfo → `CellInfo::registerCarrier` throws "already
exists". On a UE cellInfo is null, so the shipped configs are unaffected, but the decorator is
presented as a general tool with no guard against eNB placement.

### 6. [medium] `SionnaManager::runGenerator`: unquoted `std::system` + no output check
[SionnaManager.cc](../../src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc) — the
command is raw string concatenation: a path with spaces/metacharacters (e.g. `pythonExecutable`,
`sionnaScript`, cacheDir) breaks it or **injects**; the return value is a wait-status (not the
raw exit code); and after rc==0 it immediately calls `loadTableFromFile` **without checking**
that the generator actually wrote output → a misleading "cannot open" error.

### 7. [low-medium] `SionnaManager::loadTableFromFile`: unguarded JSON access
`.at("carriers")` / `.at("links")` / `.get<...>` throw a raw `nlohmann::json` exception (not
wrapped in `cRuntimeError`) on a missing/null/mistyped field → an opaque crash on a malformed
table. (Also here: the loaded file's `granularity`/`interferenceMode` override the config —
intentional, but the coupling-guard ran with the old values.)

## Cleanup / portability / docs

### 8. [medium] The README `sionnaize.py` example targets a network that is now broken
[README.md](../../simulations/nr/sionna/README.md) Section B: `sionnaize.py ... UrbanNetwork`
— but `UrbanNetwork` **now carries** the `hasSionnaManager`/`sionnaManager` submodule, so the
generated `UrbanNetworkSionna` wrapper would **duplicate** the submodule → NED "already exists"
error. The single documented example of the wrapper path is broken (the wrapper is only valid
for networks **without** the param — e.g. `cars/Highway`).

### 9. [medium] Machine-specific absolute paths in the shared include
[sionna-common.ini](../../simulations/nr/sionna/sionna-common.ini) and
[omnetpp.ini `SionnaLive`](../../simulations/nr/sionna/omnetpp.ini) — `pythonExecutable`/
`sionnaScript` are hard-coded to `/home/zoli/...`. `sionna-common.ini` is exactly the
"include this from anywhere" shared file → on any other checkout the tworay subprocess won't
launch. (The committed `[Config Sionna]` is portable via the artifact; the live path is not.)

### 10. [low] Stale RNG-neutrality comment
[CompareChannelModel.cc:93](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.cc#L93)
— "fading and shadowing disabled (… large-scale mode) these probes draw no RNG", but the
shipped `SionnaCompare` now runs in **Full mode** (fading ON); neutrality actually holds via
the per-node/per-TTI caching, not via disabling. The comment asserts a false precondition →
misleading for a future change.

---

## Verified NOT a bug (to avoid false positives)

- `getSINR` branch/coord/noiseFigure selection matches the base in all four cases; the desired
  pair is position-based + reciprocal lookup → endpoint order doesn't matter.
- `setPhy`/`registerNode` timing: `sionnaManager_` is resolved at POSTLOCAL, `setPhy` runs at
  REGISTRATIONS2 → the guard holds.
- `getShadowingMap/getJakesMap` override called pre-init: `ModuleRefByPar::operator bool` does
  not throw on an unresolved ref.
- RNG neutrality **genuinely holds** (the probe + the internal getSINR run at the same NOW,
  cached draw).
- The `<default("SionnaManager")> like ISionnaManager if hasSionnaManager` pattern resolves in
  all 17 networks; `binder`/`carrierAggregation` references are correct.
- No repo-level CLAUDE.md → no convention violations.

---

**Focus:** #1 (wrong bg/ext interference under Sionna) and #2/#3 (sionna_rt.py edge case +
backend consistency) are the most substantive correctness items; #8/#9 are doc/portability
issues that hit the user directly. None blocks the current (single-cell, static,
committed-artifact) use — the 129-OK fingerprint suite shows that — but they surface when
extending to background cells / multi-carrier / live Sionna.
