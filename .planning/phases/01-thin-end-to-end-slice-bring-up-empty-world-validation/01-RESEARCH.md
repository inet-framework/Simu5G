# Phase 1: Thin End-to-End Slice (Bring-Up & Empty-World Validation) - Research

**Researched:** 2026-06-17
**Domain:** OMNeT++/Simu5G channel-model polymorphism (C++), offline Sionna RT 2.0.1 precompute (Python), artifact exchange format, Friis free-space calibration
**Confidence:** HIGH

## Summary

This phase builds the thinnest complete vertical slice of the Simu5G × Sionna integration: a single shared scenario (SSOT) drives an offline Sionna RT tool over an empty world with one Tx/Rx link, which emits a versioned artifact + manifest, which a new `SionnaChannelModel` C++ class loads, validated by an empty-world Friis round-trip. Every architectural seam is exercised exactly once.

The single most important finding is empirical and decisive: I ran the actual Sionna 2.0.1 venv against an empty scene and confirmed that **Sionna's empty-world LOS path gain agrees with the textbook Friis value to within ~0.04 dB** (Sionna: -83.36 dB; Friis: -83.32 dB at 100 m / 3.5 GHz). The walking-skeleton validation gate is therefore proven to be achievable — the ~0.5–1 dB tolerance in the success criteria is comfortably met, and the residual is far below it. `[VERIFIED: sionna venv run]`

The second decisive finding is that the channel-model seam **already exists** and requires zero NED interface edits. Simu5G instantiates `channelModel[numCarriers]: <lteChannelModelType> like ILteChannelModel` (and the NR equivalent `nrChannelModel[numNrCarriers]: <nrChannelModelType>`). The `<...>` is a NED *parametric typename*. Setting the ini string `*.*.cellularNic.nrChannelModelType = "SionnaChannelModel"` selects a new `@class("SionnaChannelModel")` simple module with no NED edit — exactly satisfying SEAM-01. `[VERIFIED: codebase grep]`

The third decisive finding affects ART-01/ART-02: **HDF5 and HighFive are NOT installed system-wide** on this machine (`pkg-config --exists hdf5` fails; no `/usr/include/highfive`). Adding an HDF5 C++ reader to the Simu5G build therefore introduces a new system dependency that directly threatens the byte-for-byte default-build constraint (SEAM-02). For a thin slice, the **binary+JSON-manifest variant** (offline tool writes HDF5 as canonical/debug *plus* a small little-endian binary table and a JSON manifest the C++ reads) is the lower-risk choice. CLAUDE.md already lists this as a sanctioned variant.

**Primary recommendation:** Subclass `SionnaChannelModel : public NrChannelModel`, override **`getAttenuation()`** to return `-pathGain_dB` from a loaded table (so the inherited `getSINR()`/`getRSRP()`/interference machinery is reused unchanged per MOD-01); ship the offline tool writing a **JSON manifest + little-endian binary path-gain table** (no HDF5 C++ dep); add a lightweight `SionnaManager` (or in-model init hook) that asserts the manifest contract at `INITSTAGE_LOCAL` and throws `cRuntimeError` on mismatch; validate with a host-side Friis comparison harness in the offline tool and an OMNeT++ fingerprint.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SEAM-01 | Select Sionna model via ini string, no NED interface change | `<lteChannelModelType>`/`<nrChannelModelType>` parametric typename in `LteNicBase.ned:101` / `NrNicUe.ned:71`; ini override demonstrated in `simulations/nr/mec/singleMecHost/omnetpp.ini:59`. New `@class("SionnaChannelModel")` simple module + ini string suffices. |
| SEAM-02 | Default build/run byte-for-byte unaffected; no Python/HDF5 in default binary | Build is `opp_makemake --deep` over `src/`; new files would be auto-compiled into the default `.so`. Must guard Sionna C++ behind a `.oppfeatures` feature flag or preprocessor guard so default build links zero new symbols. HDF5/HighFive absent system-wide → use binary+JSON reader (no HDF5 C++ dep) to keep default binary clean. |
| TOOL-01 | Shared scenario (SSOT) drives both tool and sim | Single config file (JSON/YAML) describing positions, antenna heights, carrier freq, numerology, band count, materials, MCS set; consumed by the Python tool and asserted against by the C++ model. v1: one Tx, one Rx. |
| TOOL-02 | Versioned coord/units transform between OMNeT++ and Sionna | OMNeT++ INET `Coord` is metres, x-east/y-north/z-up; Sionna scene is metres, right-handed. v1 with empty world: identity transform (offset+axis map) recorded explicitly in the manifest `coord_transform` block. |
| TOOL-03 | One batched `PathSolver` extracting per-link path gains via `Paths.cfr` over `subcarrier_frequencies` | Verified API: `PathSolver.__call__(scene, max_depth, los, specular_reflection, ...)` → `Paths`; `subcarrier_frequencies(num_subcarriers, subcarrier_spacing)`; `Paths.cfr(frequencies, normalize=False, out_type='numpy')`. Path gain = `mean(|H|^2)`. `[VERIFIED: sionna venv run]` |
| ART-01 | HDF5 artifact + JSON manifest with `schema_version` checked at load | Offline tool writes HDF5 (canonical/debug, h5py 3.16.0 present) + JSON manifest. C++ reads JSON manifest + binary table (HDF5 C++ dep avoided). `schema_version` is first field asserted. |
| ART-02 | Manifest carries full parameter contract, `coord_transform`, request hash; reserves degenerate SINR-bin axis | Manifest JSON schema below. Path-gain table shaped `[L]` (v1) but documented as the `S=1` degenerate slice of v2's `[L, S]` over a `sinr_grid[S]` — additive extension. |
| MOD-01 | `SionnaChannelModel` retains inherited `getSINR()` interference+noise aggregation | Override only `getAttenuation()` (returns dB attenuation = -pathGain). `getSINR()` in `LteRealisticChannelModel.cc:514` does `recvPower -= attenuation` then adds interference/noise — fully reused. `[VERIFIED: codebase read]` |
| CAL-01 | Empty-world reference mode validates Sionna path gain vs Friis within ~0.5–1 dB | Empirically confirmed: 0.04 dB residual at 100 m / 3.5 GHz. `[VERIFIED: sionna venv run]` |
| CAL-02 | `SionnaManager` asserts manifest vs live scenario at init, fails loud (`cRuntimeError`) | Assert at `INITSTAGE_LOCAL` (pattern in `LteRealisticChannelModel.cc:46`); `throw cRuntimeError(...)` pattern used throughout (`NrChannelModel.cc:161`). Compare `schema_version`, carrier freq, numerology/SCS, band count, antenna config, Tx-power convention, `coord_transform`, request hash. |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Scene authoring (SSOT) | Offline tool (Python) | Config file (shared) | The shared scenario is data; the Python tool consumes it and the sim asserts against the same source. |
| Ray tracing → per-link path gain | Offline tool (Sionna RT, Python) | — | Hard constraint: no Python/GPU in the sim runtime. RT runs offline, once. |
| Artifact emission (HDF5 + manifest + binary) | Offline tool (Python) | — | h5py/numpy live only in the venv. |
| Artifact loading + contract assertion | Simu5G C++ (`SionnaManager` / model init) | — | Must run in the default-buildable C++ side; reads JSON + binary, no HDF5 C++ dep. |
| Path-gain substitution | Simu5G C++ (`SionnaChannelModel::getAttenuation`) | inherited `NrChannelModel` | Sionna owns path gain; Simu5G owns the SINR value (interference+noise). |
| SINR/RSRP/interference aggregation | Simu5G C++ (inherited `getSINR`/`getRSRP`) | — | MOD-01: unchanged inherited behaviour. |
| Friis validation | Offline tool (Python harness) | OMNeT++ fingerprint | Numerical cross-check lives where both Sionna value and analytic Friis are available; sim-side reproducibility via pinned fingerprint. |
| Default-build isolation | Build system (`.oppfeatures` / makefrag) | — | SEAM-02 is a build-tier concern, not a code-tier one. |

## Standard Stack

### Core (offline tool — pre-provisioned venv at `/home/zoli/Projects/OMNET/Sionna/venv`)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| sionna-rt | 2.0.1 | Empty-world scene, `PathSolver`, `Paths.cfr`, `subcarrier_frequencies` | The RT engine; TF-free (Dr.Jit/Mitsuba). `[CITED: CLAUDE.md]` `[VERIFIED: venv introspection]` |
| sionna | 2.0.1 | (phy/sys reserved for Phase 2 BLER) | Same venv; not exercised in Phase 1. `[CITED: CLAUDE.md]` |
| numpy | 2.4.6 | `out_type='numpy'` extraction, write binary/HDF5 | Universal glue. `[VERIFIED: venv]` |
| h5py | 3.16.0 | Write canonical HDF5 artifact (debug/reference) | Self-describing artifact. `[VERIFIED: venv]` |
| mitsuba / drjit | 3.8.0 / 1.3.1 | Auto-selected RT backend (LLVM CPU here, no GPU dep needed) | Pulled by sionna-rt; leave variant auto-selected. `[CITED: CLAUDE.md]` |

### Supporting (Simu5G C++ side)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| OMNeT++ `cValueMap`/`cValueArray` + `cConfiguration` | bundled | Could parse small JSON-ish config | Optional; a hand-rolled minimal JSON reader or a single-header JSON lib vendored under the feature dir is also fine since it only compiles when the feature is on. |
| INET `inet::Coord` | bundled | Node positions (metres) for distance + transform | Already the coordinate type returned by `LtePhyBase::getCoord()`. `[VERIFIED: LtePhyBase.h:332]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Binary+JSON manifest (C++ reader) | HighFive header-only HDF5 reader | HighFive avoids the HDF5 C++ API but still needs the HDF5 **C** library, which is absent here → new system dep, risks SEAM-02. Binary+JSON has zero new system deps. **Choose binary+JSON for the thin slice.** |
| Override `getAttenuation()` | Override `computePathLoss()` | `computePathLoss` has different signatures in LTE vs NR base and excludes shadowing/angular terms; `getAttenuation` is the single clean entry that all of `getSINR`/`getRSRP` funnel through. **Choose `getAttenuation`.** |
| `Paths.cfr` per-subcarrier | `RadioMapSolver`/`RadioMap.path_gain` | Radio maps are the wide-area calibration cross-check (Phase 4). For one precise link, `cfr` is direct. `[CITED: CLAUDE.md]` |
| `.oppfeatures` feature flag | Preprocessor `#ifdef WITH_SIONNA` guard | Feature flag integrates with `opp_makemake`/IDE and cleanly excludes source folders from the default build; preprocessor guard alone still compiles (empty) TUs. Prefer the feature flag, optionally combined with a guard. |

**Installation:** No installation needed for Phase 1 — the Python venv is pre-provisioned with the pinned versions above; the C++ side adds no external library.

**Version verification:** All Python versions verified live by importing in the venv (`sionna.__version__ == '2.0.1'`). `[VERIFIED: venv]`

## Package Legitimacy Audit

> No new external packages are installed by this phase. The Python venv at `/home/zoli/Projects/OMNET/Sionna/venv` is pre-existing and its versions are pinned in CLAUDE.md and verified by live introspection. The Simu5G C++ side adds **zero** external libraries (binary+JSON reader is hand-rolled or vendored header-only inside the feature dir).

| Package | Registry | Source Repo | Verdict | Disposition |
|---------|----------|-------------|---------|-------------|
| sionna-rt 2.0.1 | PyPI (pre-installed) | github.com/NVlabs/sionna | OK | Approved (pre-provisioned, pinned) |
| sionna 2.0.1 | PyPI (pre-installed) | github.com/NVlabs/sionna | OK | Approved (pre-provisioned, pinned) |
| h5py / numpy | PyPI (pre-installed) | well-known | OK | Approved |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

*If the binary+JSON decision is revisited and HighFive is vendored, that header-only library should be committed in-tree under the feature directory (no system install), keeping the default build clean.*

## Architecture Patterns

### System Architecture Diagram

```
                    SHARED SCENARIO (SSOT)  [TOOL-01]
                    positions, antennas, fc, SCS,
                    band count, materials, MCS set,
                    coord_transform
                           |
              +------------+-------------+
              |                          |
              v                          v
   OFFLINE PYTHON TOOL              SIMU5G C++ (default-buildable)
   (venv, run once)                 reads same SSOT for assertion
   1. load_scene() empty world
   2. PlanarArray tx/rx
   3. add Transmitter, Receiver
   4. PathSolver(max_depth=0..N, los=True)
   5. subcarrier_frequencies(nRB*..., SCS)
   6. Paths.cfr -> H (numpy)
   7. path_gain_dB = 10log10(mean(|H|^2))
   8. Friis cross-check (host harness) [CAL-01]
              |
              v
   ARTIFACT WRITE  [ART-01/02]
   - canonical: results.h5  (h5py, debug)
   - manifest: manifest.json
       schema_version, coord_transform,
       parameter contract, request_hash,
       sinr_grid (degenerate S=1)
   - table: path_gain.bin (LE float64 [L])
              |
              v   (file on disk; no Python at sim runtime)
   ====================================================
   SIMU5G RUN (default-buildable binary, Sionna feature ON)
              |
              v
   SionnaManager::initialize(INITSTAGE_LOCAL)  [CAL-02]
   - read manifest.json
   - assert schema_version == expected
   - assert fc / SCS / band count / antennas /
            tx-power convention / coord_transform
            == live scenario  -> else cRuntimeError
   - mmap/read path_gain.bin -> in-memory table
              |
              v
   SionnaChannelModel : public NrChannelModel   [MOD-01]
   getAttenuation(nodeId,dir,coord,cqiDl):
       d = phy_->getCoord().distance(coord)   (transform applied)
       return -table.pathGain_dB(link)        // dB attenuation
              |  (inherited path)
              v
   NrChannelModel/LteRealisticChannelModel::getSINR / getRSRP
   recvPower -= attenuation; + antennaGain; - cableLoss;
   + interference + noise   (UNCHANGED, Simu5G owns SINR)
              |
              v
   single-link simulation runs; rcvdSinrDl reflects Sionna path gain
```

### Recommended Project Structure
```
src/simu5g/stack/phy/channelmodel/sionna/   # guarded by .oppfeatures feature flag
├── SionnaChannelModel.{h,cc,ned}   # : NrChannelModel, @class, overrides getAttenuation
├── SionnaManager.{h,cc,ned}        # loads manifest+binary, asserts contract, owns table
├── SionnaTable.{h,cc}              # in-memory [L] path-gain lookup (v1) / [L,S] (v2)
└── ManifestReader.{h,cc}           # minimal JSON + LE-binary reader (no HDF5 dep)

tools/sionna_precompute/            # offline Python tool (outside Simu5G build)
├── precompute.py                   # SSOT -> PathSolver -> path gain -> artifact
├── friis_check.py                  # CAL-01 cross-check harness
├── scenario.example.json           # SSOT example
└── requirements.txt                # pin venv versions (sionna-rt==2.0.1 ...)
```

### Pattern 1: Parametric-typename channel-model selection (SEAM-01)
**What:** Simu5G's NIC NED already declares the channel model with a parametric type.
**When to use:** Always — this is the entire SEAM-01 mechanism.
```ned
// src/simu5g/stack/LteNicBase.ned:48,101  [VERIFIED: codebase]
string lteChannelModelType = default("LteRealisticChannelModel");
...
channelModel[numCarriers]: <lteChannelModelType> like ILteChannelModel { ... }

// src/simu5g/stack/NrNicUe.ned:33,71
string nrChannelModelType = default("NrChannelModel_3GPP38_901");
...
nrChannelModel[numNrCarriers]: <nrChannelModelType> like ILteChannelModel { ... }
```
Selection from ini (no NED edit):
```ini
*.*.cellularNic.nrChannelModelType = "SionnaChannelModel"   # NR
*.*.cellularNic.lteChannelModelType = "SionnaChannelModel"  # LTE
```
The new module only needs `@class("SionnaChannelModel")` and to `extend NrChannelModel` in NED so it satisfies `like ILteChannelModel`.

### Pattern 2: Path-gain override that reuses all inherited machinery (MOD-01)
**What:** Override the single dB-attenuation entry point; let `getSINR`/`getRSRP` stay untouched.
```cpp
// getSINR consumes attenuation here  [VERIFIED: LteRealisticChannelModel.cc:506-521]
attenuation = getAttenuation(ueId, dir, coord, cqiDl); // dB
recvPower -= attenuation;       // (dBm - dB) = dBm
recvPower += antennaGainTx;     // ...inherited, unchanged
recvPower -= cableLoss_;
// Override returns -pathGain_dB so recvPower reflects Sionna geometry:
double SionnaChannelModel::getAttenuation(MacNodeId id, Direction dir,
                                          inet::Coord coord, bool cqiDl) override {
    double pathGain_dB = table_->lookup(linkKeyFor(id, dir, coord)); // from artifact
    return -pathGain_dB; // attenuation (dB) = -gain (dB)
}
```
Note: to satisfy MOD-02 fully (no double-counting) is a **Phase 3** concern; for Phase 1's thin slice, set `shadowing=false`, `fading=false`, `dynamicLos=false`/`fixedLos=true` in the ini for the Sionna config so the inherited statistical terms are inert and the Friis round-trip is clean.

### Pattern 3: Fail-loud manifest assertion at init (CAL-02)
```cpp
// pattern: LteRealisticChannelModel.cc:46 (INITSTAGE_LOCAL) + cRuntimeError throws
void SionnaManager::initialize(int stage) {
    if (stage == inet::INITSTAGE_LOCAL) {
        Manifest m = ManifestReader::read(par("artifactManifest").stringValue());
        if (m.schema_version != EXPECTED_SCHEMA_VERSION)
            throw cRuntimeError("Sionna manifest schema_version %d != expected %d",
                                m.schema_version, EXPECTED_SCHEMA_VERSION);
        assertContractMatchesLiveScenario(m); // fc, SCS, bands, antennas, txpow, transform
        table_ = SionnaTable::loadBinary(m.tablePath, m);
    }
}
```

### Pattern 4: Empty-world path gain in the offline tool (TOOL-03, CAL-01)
```python
# Source: verified live against sionna-rt 2.0.1 venv  [VERIFIED]
import numpy as np, math
from sionna.rt import load_scene, PathSolver, PlanarArray, Transmitter, Receiver, subcarrier_frequencies
scene = load_scene()                       # empty world (no geometry)
scene.frequency = 3.5e9
scene.tx_array = PlanarArray(num_rows=1, num_cols=1, pattern='iso', polarization='V')
scene.rx_array = PlanarArray(num_rows=1, num_cols=1, pattern='iso', polarization='V')
scene.add(Transmitter('tx', [0,0,10]))     # positions = OMNeT++ coords via coord_transform
scene.add(Receiver('rx', [100,0,1.5]))
paths = PathSolver()(scene=scene, max_depth=0, los=True,
                     specular_reflection=False, refraction=False, synthetic_array=False)
freqs = subcarrier_frequencies(1, 30e3)    # v1 single representative subcarrier; expand to nRB
H = paths.cfr(frequencies=freqs, normalize=False, out_type='numpy')
pathgain_lin = float(np.mean(np.abs(H)**2))
pathgain_dB  = 10*math.log10(pathgain_lin) # e.g. -83.36 dB
# Friis cross-check:
d, lam = 100.0, 3e8/3.5e9
friis_dB = 20*math.log10(lam/(4*math.pi*d)) # -83.32 dB ; residual 0.04 dB
assert abs(pathgain_dB - friis_dB) < 1.0    # CAL-01 gate
```

### Anti-Patterns to Avoid
- **Embedding Python/Sionna in the sim runtime:** violates the hard constraint; the artifact is the only coupling. `[CITED: CLAUDE.md "What NOT to Use"]`
- **Linking HDF5 into the default Simu5G binary:** HDF5 is absent system-wide here; adding it risks SEAM-02. Read binary+JSON in C++. `[VERIFIED: pkg-config]`
- **Overriding `computePathLoss` instead of `getAttenuation`:** different signatures LTE vs NR and bypasses shadowing/angular accounting in `getSINR`.
- **Editing `ILteChannelModel.ned` / the NIC NED to add the model:** unnecessary — parametric typename already supports it; editing it would break SEAM-01's "no NED interface change".
- **Using `Paths.cfr(..., normalize=True)`:** normalizes away the absolute gain we need for Friis; use `normalize=False`. `[VERIFIED: cfr signature]`
- **JSON for the bulk path-gain table:** lossy on floats; JSON only for the small manifest. `[CITED: CLAUDE.md]`

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Ray-traced LOS path gain | Custom RT or Friis-only code in the tool | `sionna.rt.PathSolver` + `Paths.cfr` | This *is* the differentiator; Friis is only the empty-world sanity check, not the model. |
| Channel-model selection plumbing | New NED interface / factory | Existing `<typename> like ILteChannelModel` | Already polymorphic; ini string is the switch. `[VERIFIED]` |
| SINR/interference/noise aggregation | New SINR math in SionnaChannelModel | Inherited `NrChannelModel::getSINR` | MOD-01 mandates reuse; only path gain changes. |
| Sub-carrier frequency grid | Manual `fc + k*SCS` loop | `subcarrier_frequencies(n, SCS)` | Verified helper; matches Sionna's internal convention. |
| dBm/linear conversions | New helpers | Existing `dBmToLinear`/`linearToDBm` in base | Used throughout `getSINR`/`getRSRP`. `[VERIFIED]` |

**Key insight:** The thin slice is almost entirely *wiring*: one new override, one loader, one offline script. The hard physics (RT, link budget) is already done — by Sionna on one side and by Simu5G's inherited `getSINR` on the other.

## Runtime State Inventory

> Phase 1 is greenfield additive (new files + new ini string), not a rename/refactor. No existing runtime state is renamed or migrated. The one cross-process state item is the **artifact on disk** (manifest.json + path_gain.bin + results.h5), which is new and produced by the offline tool.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no existing datastore touched. New artifact files are created fresh. | none |
| Live service config | None — no external service. | none |
| OS-registered state | None. | none |
| Secrets/env vars | None — venv path is a fixed dev path; no secrets. | none |
| Build artifacts | Default `bin/simu5g` / `out/` rebuild; must stay byte-for-byte identical with feature OFF (SEAM-02). Stale `.o` under `out/` should be cleaned before the baseline diff. | clean + rebuild baseline before/after; diff binaries with feature off |

**Nothing found in categories 1–4:** verified — Phase 1 adds files and one ini string; it does not rename or migrate any existing identifier, key, or registration.

## Common Pitfalls

### Pitfall 1: Default build silently picks up Sionna sources (SEAM-02 break)
**What goes wrong:** `opp_makemake --deep` globs all of `src/`; new `sionna/*.cc` get compiled into the default `.so`, changing the binary.
**Why it happens:** The build is deep/auto; there is no per-file opt-out by default.
**How to avoid:** Put Sionna C++ under a folder declared in `.oppfeatures` as an `extraSourceFolders` of a `Simu5G_Sionna` feature (`initiallyEnabled="false"`), mirroring the existing `Simu5G_Cars` feature. Confirm with `nm -D bin/libsimu5g.so | grep -i 'sionna\|hdf5\|python'` returns nothing in a default build.
**Warning signs:** Binary size changes; `nm` shows new symbols; fingerprint of an existing config changes.

### Pitfall 2: Coordinate/units transform mismatch (TOOL-02) corrupts everything plausibly
**What goes wrong:** OMNeT++ Coord (metres, x-east/y-north/z-up) vs Sionna scene coords differ by an offset, axis swap, or handedness; path gain is computed for the wrong distance but still "looks plausible."
**Why it happens:** Silent — no error, just wrong numbers.
**How to avoid:** v1 empty world: use an explicit identity-or-offset `coord_transform` recorded in the manifest, and make CAL-01 compare Sionna gain at the **OMNeT++ Euclidean distance** (`phy_->getCoord().distance(coord)`), which is exactly what the inherited code computes (`NrChannelModel.cc:34`). The Friis residual being ~0.04 dB is the transform-correctness proof.
**Warning signs:** Friis residual > 1 dB at a known distance.

### Pitfall 3: Inherited statistical terms leak into the Sionna path (MOD-01/MOD-02)
**What goes wrong:** Shadowing/fading/LOS draw add stochastic dB on top of Sionna's deterministic gain, breaking the Friis round-trip and determinism.
**Why it happens:** `getAttenuation` in the base adds `computeShadowing(...)`; `getSINR` adds fading per band.
**How to avoid (Phase 1 scope):** In the Sionna ini config set `shadowing=false`, `fading=false`, `dynamicLos=false`, `fixedLos=true`. Full source-level suppression is the Phase 3 deliverable (MOD-02); Phase 1 only needs the override to return the Sionna gain and the ini to silence stochastic terms.
**Warning signs:** RSRP differs across seeds (REP-02 will later catch this); Friis residual non-deterministic.

### Pitfall 4: `cfr` normalization or wrong subcarrier count distorts absolute gain
**What goes wrong:** `normalize=True` (or averaging over a band that includes near-zero subcarriers) changes the absolute path gain.
**How to avoid:** `normalize=False`; for v1 use a single representative subcarrier at band center (or average `|H|^2` over the full RB grid consistently and document it as the per-link wideband figure per design plan §5).
**Warning signs:** Path gain off by a constant offset vs Friis.

### Pitfall 5: Manifest assertion too weak → silent contract drift (CAL-02)
**What goes wrong:** Only `schema_version` is checked; a carrier-freq or band-count mismatch passes silently.
**How to avoid:** Assert every contract field listed in the manifest schema; throw `cRuntimeError` on the first mismatch. No silent fallback to the analytic model.

## Code Examples

### Empty-world path gain extraction (verified)
See Pattern 4 above — run live against the venv with the exact output `path gain dB -83.36041`, `friis dB -83.3231`.

### NED for the new model (skeleton)
```ned
// src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.ned
package simu5g.stack.phy.channelmodel.sionna;
import simu5g.stack.phy.channelmodel.NrChannelModel;
simple SionnaChannelModel extends NrChannelModel {
    parameters:
        @class("SionnaChannelModel");
        string sionnaManagerModule = default("^.^.sionnaManager"); // or read artifact directly
        string artifactManifest = default("sionna_artifact/manifest.json");
}
```

### Default-build isolation feature (mirrors `Simu5G_Cars`)
```xml
<!-- .oppfeatures : add a feature -->
<feature id="Simu5G_Sionna" name="Simu5G Sionna" initiallyEnabled="false"
         extraSourceFolders="src/simu5g/stack/phy/channelmodel/sionna"
         compileFlags="-DWITH_SIONNA" nedPackages="simu5g.stack.phy.channelmodel.sionna"/>
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Sionna 0.x–1.x (TensorFlow, `sionna.channel`, `sn.rt` TF objects) | Sionna 2.x (Dr.Jit/Mitsuba RT + PyTorch PHY/SYS) | 2.0 (2025); 2.0.1 2026-03-31 | Training-data TF examples won't run; use the verified 2.x API only. `[CITED: CLAUDE.md]` |
| `PhyPisaData::getBler` generic AWGN/TU tables | (Phase 2) `sionna.sys.PHYAbstraction` site-specific BLER | Phase 2, not Phase 1 | Phase 1 substitutes path gain only; BLER stays analytic until Phase 2. |

**Deprecated/outdated:**
- TensorFlow / any TF-era Sionna API: not installed, must not be added. `[CITED: CLAUDE.md]`

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Setting `nrChannelModelType`/`lteChannelModelType` ini string is sufficient to instantiate `SionnaChannelModel` with no NED edit, *given* the NED is registered via `@class` and `like ILteChannelModel`. | SEAM-01 / Pattern 1 | Low — mechanism is verified in code and used in an existing ini; only the new module's NED registration must be correct. |
| A2 | The binary+JSON variant is lower-risk than HighFive for the thin slice. | ART-01 / Stack | Low — driven by verified absence of HDF5 system libs; if HDF5 is later installed deliberately, HighFive becomes viable. Needs user confirmation of the format choice. |
| A3 | Setting `shadowing/fading=false`, `fixedLos=true` in the Sionna ini config is an acceptable Phase-1 way to inert statistical terms (deferring full MOD-02 suppression to Phase 3). | Pitfall 3 / Pattern 2 | Medium — confirm with user that Phase 1 may rely on ini-level suppression rather than source-level. Roadmap explicitly assigns MOD-02 to Phase 3, so this is consistent. |
| A4 | v1 represents the link with a single (or wideband-averaged) subcarrier path gain; the per-RB grid is collapsed to one figure. | Pattern 4 / design plan §5 | Low — explicitly documented design decision; confirm averaging convention is pinned identically in manifest. |
| A5 | OMNeT++/INET Coord is metres, x-east/y-north/z-up, mapping to Sionna scene metres via an identity/offset transform in the empty world. | TOOL-02 | Low — standard INET convention; the Friis residual is the empirical proof of correctness. |
| A6 | A dedicated `SionnaManager` module (vs. loading inside the model) is the cleaner owner of the global table + assertion. | Architecture | Low — design plan §4.1 already raises this as a choice (Binder vs SionnaManager); either works for one link. Confirm preference with planner/user. |

## Open Questions (RESOLVED)

1. **Artifact format final call (HDF5-only-debug vs binary+JSON for C++).**
   - What we know: HDF5/HighFive absent system-wide; CLAUDE.md sanctions both the HDF5 path and the binary+JSON variant.
   - What's unclear: whether the team wants to install HDF5 deliberately to use HighFive.
   - Recommendation: binary+JSON for the C++ reader in Phase 1; keep HDF5 as the canonical/debug artifact written by the tool. Revisit if HDF5 is adopted as a build dep.
   - **RESOLVED:** binary+JSON for the C++ reader (no HDF5 dependency in the default build); HDF5 stays canonical/debug in the Python tool only. Decision lives in SKELETON.md Architectural Decisions and is implemented by Plan 01-01 (Task 3 writes `manifest.json` + LE-binary `path_gain.bin` + `results.h5`) and consumed by Plan 01-03 (Task 1 `ManifestReader`/`SionnaTable`, no HDF5 C++ dep).

2. **Manager ownership (`SionnaManager` module vs in-model loader).**
   - What we know: one link → trivial either way; for N links one global table is cheaper (design plan §4.1).
   - Recommendation: introduce a thin `SionnaManager` now (forward-compatible with Phase 2/3) but keep it minimal.
   - **RESOLVED:** a thin `SionnaManager` module owns the table + contract assertion. Decision lives in SKELETON.md and is implemented in Plan 01-02 (skeleton) + Plan 01-03 (contract assertion at `INITSTAGE_LOCAL`).

3. **Subcarrier representation of the per-link gain.**
   - What we know: `cfr` returns per-subcarrier H; v1 collapses to one figure.
   - Recommendation: pin "band-center single subcarrier" or "mean |H|^2 over all RBs" in the manifest and use the identical convention in CAL-01.
   - **RESOLVED:** single representative subcarrier at band center, pinned as `subcarrier_representation = "single-subcarrier-band-center"` in the manifest (Plan 01-01 Task 3) and matched identically by CAL-01 (Plan 01-01 Tasks 1–2 / `friis_check.py`).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Python venv (sionna-rt, h5py, numpy) | Offline tool (TOOL-03, CAL-01) | ✓ | sionna 2.0.1 / numpy 2.4.6 / h5py 3.16.0 | — |
| Dr.Jit LLVM CPU backend | RT without GPU | ✓ | drjit 1.3.1 | GPU not required for one link |
| OMNeT++ / INET build toolchain | Simu5G build | ✓ (existing repo builds) | per repo | — |
| HDF5 C/C++ library + HighFive | (only if HDF5 C++ reader chosen) | ✗ | — | **binary+JSON reader (chosen) — no system dep** |
| `nm` / `objdump` | SEAM-02 symbol verification | ✓ | system | — |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** HDF5/HighFive — avoided entirely by the binary+JSON reader choice (directly supports SEAM-02).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | OMNeT++ fingerprint tests (`tests/fingerprint/`) for the sim; a small Python harness (`tools/sionna_precompute/friis_check.py`, pytest-style or assert-based) for CAL-01 |
| Config file | `tests/fingerprint/` runner exists in-repo; Python harness new (Wave 0) |
| Quick run command | `python tools/sionna_precompute/friis_check.py` (CAL-01 residual gate) |
| Full suite command | OMNeT++ run of the single-link Sionna config + fingerprint compare; `nm` symbol check for default build |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SEAM-01 | ini string selects SionnaChannelModel | smoke | run config with `nrChannelModelType="SionnaChannelModel"`; assert module class instantiated | ❌ Wave 0 (new sim config) |
| SEAM-02 | default binary has no Sionna/HDF5/Python symbols | unit | `nm -D bin/libsimu5g.so \| grep -iE 'sionna\|hdf5\|python' \| wc -l` == 0 (feature off) | ❌ Wave 0 (script) |
| SEAM-02 | default run byte-for-byte vs baseline | fingerprint | OMNeT++ fingerprint of an existing standalone config unchanged | ✅ `tests/fingerprint/` |
| TOOL-03 | empty-world path gain extracted | unit (Python) | `python -c` extracting `Paths.cfr` (verified) | ❌ Wave 0 |
| CAL-01 | Friis residual < ~1 dB | unit (Python) | `friis_check.py` asserts `abs(sionna_dB - friis_dB) < 1.0` | ❌ Wave 0 |
| CAL-02 | manifest mismatch aborts | integration | run with a deliberately wrong manifest field; expect `cRuntimeError` (nonzero exit) | ❌ Wave 0 |
| ART-01/02 | artifact has schema_version, coord_transform, contract, hash, degenerate sinr axis | unit (Python) | assert manifest JSON keys present | ❌ Wave 0 |
| MOD-01 | inherited getSINR reused | integration | single-link run produces `rcvdSinrDl` consistent with Sionna gain + analytic noise | ❌ Wave 0 (new config) |

### Sampling Rate
- **Per task commit:** `python tools/sionna_precompute/friis_check.py` (fast, no GPU).
- **Per wave merge:** build with feature ON + run single-link Sionna config; build with feature OFF + `nm` symbol check + existing fingerprint.
- **Phase gate:** all of the above green + the new Sionna config has a pinned fingerprint baseline (REP-01 is Phase 3, but Phase 1 should at least record the baseline).

### Wave 0 Gaps
- [ ] `tools/sionna_precompute/precompute.py` + `friis_check.py` — covers TOOL-03, CAL-01, ART-01/02
- [ ] `tools/sionna_precompute/requirements.txt` — pin venv versions
- [ ] `simulations/nr/sionna/omnetpp.ini` + a single-link network (reuse `SingleCell_Standalone`) — covers SEAM-01, MOD-01, CAL-02
- [ ] `tests/` script: `nm` default-build symbol check — covers SEAM-02
- [ ] Pinned fingerprint baseline for the Sionna single-link config

## Security Domain

> `security_enforcement: true`, ASVS L1. This is offline scientific-simulation tooling with no network service, no authN/Z, no user-facing input surface. Most ASVS categories are N/A; the relevant risk is untrusted-file parsing.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth surface (local sim + offline tool) |
| V3 Session Management | no | No sessions |
| V4 Access Control | no | Local files only |
| V5 Input Validation | yes | The C++ manifest/binary reader parses files; validate `schema_version`, array lengths, and field types before use; fail with `cRuntimeError` on malformed input (also satisfies CAL-02). Bound the binary table read by the declared length; reject out-of-range/negative sizes. |
| V6 Cryptography | no (integrity only) | The `request_hash` is an integrity/cache key, not a security control; a non-cryptographic or SHA-256 hash is fine. Do not treat it as tamper protection. |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed/oversized binary table → buffer over-read | Tampering / DoS | Validate declared length against file size; bounds-check before indexing; use `std::vector`/`std::ifstream` reads sized by validated header. |
| Manifest field type confusion (string where number expected) | Tampering | Strict typed parsing; throw on type mismatch. |
| Path traversal via artifact path ini param | Tampering | Treat artifact paths as trusted local config (research tooling); optionally restrict to project-relative paths. Low risk in this domain. |

## Sources

### Primary (HIGH confidence)
- Live Simu5G codebase reads/greps: `LteNicBase.ned`, `NrNicUe.ned`, `NrNicEnb.ned`, `LteRealisticChannelModel.{h,cc}`, `NrChannelModel.{h,cc}`, `ILteChannelModel.ned`, `LtePhyBase.{ned,h}`, `src/Makefile`, `src/makefrag`, `.oppfeatures` — channel-model seam, override points, build isolation.
- Live Sionna 2.0.1 venv introspection + execution at `/home/zoli/Projects/OMNET/Sionna/venv`: `PathSolver.__call__`, `Paths.cfr`, `Paths.cir`, `subcarrier_frequencies` signatures; empty-world path-gain run producing -83.36 dB vs Friis -83.32 dB.
- `pkg-config --exists hdf5` (fails) and `/usr/include` scan — HDF5/HighFive absence.

### Secondary (MEDIUM confidence)
- `Sionna/sionna-integration-plan.md`, `Sionna/rt_step1.py`, `Sionna/rt_step2.py` — design intent and API usage patterns.
- `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`, `.planning/STATE.md` — phase scope and constraints.

### Tertiary (LOW confidence)
- CLAUDE.md stack table (versions cross-confirmed against live venv → effectively HIGH for the items I re-verified).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all Python versions verified live; C++ side adds nothing.
- Architecture (seam + override point): HIGH — verified in source; the parametric typename and `getAttenuation` funnel are read directly.
- Friis validation feasibility: HIGH — empirically run; 0.04 dB residual.
- Artifact format choice: MEDIUM — driven by verified HDF5 absence; final format is a confirmable decision (A2).
- Pitfalls: HIGH — derived from read source and the verified build mechanism.

**Research date:** 2026-06-17
**Valid until:** 2026-07-17 (stable; Sionna 2.0.1 pinned, Simu5G seam stable)
