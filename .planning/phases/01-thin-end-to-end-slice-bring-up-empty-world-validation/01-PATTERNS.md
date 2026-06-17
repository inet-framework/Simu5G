# Phase 1: Thin End-to-End Slice (Bring-Up & Empty-World Validation) - Pattern Map

**Mapped:** 2026-06-17
**Files analyzed:** 11 (4 C++ pairs/scaffold + 1 NED + 1 .oppfeatures edit + 4 Python tool + 1 ini/network)
**Analogs found:** 10 / 11

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.{h,cc}` | model (channel) | transform (table lookup) | `src/simu5g/stack/phy/channelmodel/NrChannelModel.{h,cc}` | exact (subclass) |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.ned` | model NED | — | `src/simu5g/stack/phy/channelmodel/NrChannelModel.ned` | exact |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.{h,cc}` | service (loader/assertion) | file-I/O + init-assert | `LteRealisticChannelModel.cc::initialize` (init+throw pattern) | role-match |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.ned` | service NED | — | `NrChannelModel.ned` (`@class` simple module) | role-match |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.{h,cc}` | model (in-memory store) | transform (lookup) | (no analog — pure data holder) | none |
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.{h,cc}` | utility (parser) | file-I/O (JSON + LE binary) | (no in-tree analog; `std::ifstream` in `LteRealisticChannelModel.cc:15`) | partial |
| `.oppfeatures` (modify: add `Simu5G_Sionna`) | config (build) | — | `.oppfeatures` `Simu5G_Cars` feature | exact |
| `tools/sionna_precompute/precompute.py` | tool (offline) | batch RT → artifact | `Sionna/rt_step2.py` | role-match |
| `tools/sionna_precompute/friis_check.py` | test (Python harness) | transform (numeric assert) | RESEARCH Pattern 4 (verified snippet) | partial |
| `tools/sionna_precompute/scenario.example.json` + `requirements.txt` | config | — | CLAUDE.md version table | partial |
| `simulations/nr/sionna/omnetpp.ini` + network | config (sim) | request-response (ini override) | `NrNicUe.ned:33,71` parametric-typename wiring | exact |

## Pattern Assignments

### `SionnaChannelModel.{h,cc}` (model, transform via table lookup)

**Analog:** `src/simu5g/stack/phy/channelmodel/NrChannelModel.{h,cc}` — itself a thin subclass of `LteRealisticChannelModel`. `SionnaChannelModel` should subclass `NrChannelModel` the same way `NrChannelModel` subclasses `LteRealisticChannelModel`.

**Header pattern** (`NrChannelModel.h:13-33`) — copy the include guard + single-base inheritance + the `getAttenuation` override signature exactly:
```cpp
#ifndef NRCHANNELMODEL_H_
#define NRCHANNELMODEL_H_
#include "simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h"
namespace simu5g {
class NrChannelModel : public LteRealisticChannelModel {
  public:
    void initialize(int stage) override;
    double getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl) override;
```
New file: `class SionnaChannelModel : public NrChannelModel`, include `"simu5g/stack/phy/channelmodel/NrChannelModel.h"`, override `initialize(int)` and `getAttenuation(MacNodeId, Direction, inet::Coord, bool)`.

**Define_Module + initialize chaining** (`NrChannelModel.cc:21-26`) — the exact minimal subclass init pattern to mirror:
```cpp
Define_Module(NrChannelModel);
void NrChannelModel::initialize(int stage) {
    LteRealisticChannelModel::initialize(stage);
}
```
New: `Define_Module(SionnaChannelModel);` and `SionnaChannelModel::initialize` calls `NrChannelModel::initialize(stage)` first, then at `INITSTAGE_LOCAL` resolves its `SionnaManager`/table handle.

**Core override — distance funnel** (`NrChannelModel.cc:28-52`) — this is the exact entry point to replace. The Sionna override keeps the distance computation (proves TOOL-02 transform) but returns `-pathGain_dB` instead of `computePathLoss`:
```cpp
double NrChannelModel::getAttenuation(MacNodeId nodeId, Direction dir, inet::Coord coord, bool cqiDl) {
    double threeDimDistance = phy_->getCoord().distance(coord);   // KEEP — used for link lookup / Friis check
    ...
    double attenuation = computePathLoss(threeDimDistance, twoDimDistance, los);  // REPLACE
    if (num(nodeId) < BGUE_MIN_ID && shadowing_)
        attenuation += computeShadowing(twoDimDistance, nodeId, speed, cqiDl);    // inert in Phase 1 (shadowing=false)
    ...
}
```
New body returns `return -table_->lookup(linkKeyFor(nodeId, dir, coord));` — see RESEARCH Pattern 2. Inherited `getSINR`/`getRSRP` consume this unchanged (MOD-01).

**Why inherited machinery is reused** (`LteRealisticChannelModel.cc:509-521`) — the single funnel the override feeds; do NOT reimplement:
```cpp
attenuation = getAttenuation(ueId, dir, coord, cqiDl); // dB   <-- our override
recvPower -= attenuation;    // (dBm-dB)=dBm
recvPower += antennaGainTx;  // (dBm+dB)=dBm
recvPower += antennaGainRx;
recvPower -= cableLoss_;     // (dBm-dB)=dBm
```

---

### `SionnaChannelModel.ned` (model NED)

**Analog:** `src/simu5g/stack/phy/channelmodel/NrChannelModel.ned:21-25` — the minimal `extends ... @class(...)` form that satisfies `like ILteChannelModel` via inheritance (the parent chain already declares the interface), enabling SEAM-01 with no NED-interface edit:
```ned
package simu5g.stack.phy.channelmodel;
simple NrChannelModel extends LteRealisticChannelModel {
    parameters:
        @class("NrChannelModel");
}
```
New (`package simu5g.stack.phy.channelmodel.sionna;`):
```ned
import simu5g.stack.phy.channelmodel.NrChannelModel;
simple SionnaChannelModel extends NrChannelModel {
    parameters:
        @class("SionnaChannelModel");
        string artifactManifest = default("sionna_artifact/manifest.json");
}
```
Copy the license header block verbatim from `NrChannelModel.ned:1-13`.

---

### `SionnaManager.{h,cc}` (service: artifact loader + fail-loud contract assertion)

**Analog:** `LteRealisticChannelModel.cc:43-97` (init-at-`INITSTAGE_LOCAL` + `par(...)` reads + `throw cRuntimeError`).

**Init stage gate + param reads** (`LteRealisticChannelModel.cc:43-47`) — mirror the stage guard exactly:
```cpp
void LteRealisticChannelModel::initialize(int stage) {
    LteChannelModel::initialize(stage);
    if (stage == inet::INITSTAGE_LOCAL) {
        scenario_ = aToDeploymentScenario(par("scenario").stringValue());
        ...
```
`SionnaManager::initialize`: read manifest path from `par("artifactManifest").stringValue()` inside the `INITSTAGE_LOCAL` block.

**Fail-loud throw pattern** (`LteRealisticChannelModel.cc:80` and `NrChannelModel.cc:161,183,206,227,297`) — the exact `throw cRuntimeError(fmt, ...)` idiom used throughout the channel models; reuse for every contract field (CAL-02):
```cpp
throw cRuntimeError("Unrecognized value in 'fadingType' parameter: \"%s\"", fType.c_str());
// NrChannelModel.cc:161
throw cRuntimeError("Error: LOS indoor path loss model is valid for 3<d<150");
```
New: one `throw cRuntimeError("Sionna manifest %s mismatch: got %g, scenario has %g", ...)` per checked field (schema_version, carrierFrequency, SCS/numerology, band count, antenna config, tx-power convention, coord_transform, request hash). No silent fallback (Pitfall 5).

**Carrier-frequency / band-count live values to assert against:** `NrChannelModel.cc` uses `carrierFrequencyHz_`, `carrierFrequencyGHz_` (e.g. lines 189, 236-240, 265, 308) and `numBands_` (e.g. `NrChannelModel.cc:390`). These inherited members are the live-scenario side of the contract comparison.

---

### `SionnaManager.ned` (service NED)

**Analog:** `NrChannelModel.ned` `@class` simple-module shape (above). New: a plain `simple SionnaManager { parameters: @class("SionnaManager"); string artifactManifest = default(...); }`, placed once at NIC or network scope. Copy the license header from `NrChannelModel.ned:1-13`.

---

### `ManifestReader.{h,cc}` (utility: JSON manifest + LE-binary table reader)

**Analog (partial):** `LteRealisticChannelModel.cc:15` shows the project's file-read convention is plain `#include <fstream>` + `std::ifstream`. No JSON parser exists in-tree.

**Pattern to follow:**
- Bulk numeric table: `std::ifstream` opened `std::ios::binary`, read into `std::vector<double>` sized by a *validated* header length (V5 input-validation: bound the read by declared length; reject negative/oversized — RESEARCH Security Domain).
- Manifest JSON: OMNeT++ bundles `cValueMap`/`cValueArray` (RESEARCH Standard Stack "Supporting") for minimal typed parsing, OR vendor a single-header JSON lib *inside the feature dir* (only compiled when `Simu5G_Sionna` is ON). Throw `cRuntimeError` on type confusion / missing key.

---

### `.oppfeatures` (modify — add `Simu5G_Sionna`)

**Analog:** the existing `Simu5G_Cars` feature in `.oppfeatures:2-14` — copy its structure exactly, change `initiallyEnabled="false"` (keep), and add `extraSourceFolders` + `compileFlags` so the default deep build links zero Sionna symbols (SEAM-02):
```xml
<feature
  id="Simu5G_Cars"
  name="Simu5G Cars"
  description = "5G-enabled vehicular networks"
  initiallyEnabled = "false"
  requires = ""
  labels = ""
  nedPackages = "simu5g.simulations.lte.cars
                 simu5g.simulations.nr.cars"
  extraSourceFolders = ""
  compileFlags = ""
  linkerFlags = ""
 />
```
New feature (note: `Simu5G_Cars` leaves `extraSourceFolders` empty; the Sionna feature MUST set it to exclude the C++ from the default `.so`):
```xml
<feature
  id="Simu5G_Sionna"
  name="Simu5G Sionna"
  description = "Site-specific channel model from offline Sionna RT precompute"
  initiallyEnabled = "false"
  requires = ""
  labels = ""
  nedPackages = "simu5g.stack.phy.channelmodel.sionna"
  extraSourceFolders = "src/simu5g/stack/phy/channelmodel/sionna"
  compileFlags = "-DWITH_SIONNA"
  linkerFlags = ""
 />
```
The `<features>` root has `cppSourceRoots="src" definesFile="src/simu5g/common/features.h"` — `compileFlags="-DWITH_SIONNA"` integrates with that defines file. Verify with `nm -D bin/libsimu5g.so | grep -iE 'sionna|hdf5|python'` → empty when feature OFF.

---

### `tools/sionna_precompute/precompute.py` (offline tool)

**Analog:** `Sionna/rt_step2.py` — the verified working RT script. Copy its import block and solver invocation; strip the Munich scene/preview/render and substitute the empty world + `cfr` extraction.

**Import + variant block to copy** (`rt_step2.py:4-26`):
```python
try:
    import sionna.rt
except ImportError:
    import os; os.system("pip install sionna-rt"); import sionna.rt
import mitsuba as mi
import numpy as np
from sionna.rt import load_scene, PlanarArray, Transmitter, Receiver, Camera, \
                      PathSolver, RadioMapSolver, subcarrier_frequencies
print(f"Mitsuba variant: {mi.variant()}")   # leave variant auto-selected (do NOT override)
```

**Antenna array + solver pattern** (`rt_step2.py:33-78`) — reuse `PlanarArray(...)` and the `PathSolver()(scene=..., synthetic_array=False, ...)` call shape:
```python
scene.tx_array = PlanarArray(num_rows=1, num_cols=1, pattern="tr38901", polarization="V")
scene.rx_array = PlanarArray(num_rows=1, num_cols=1, pattern="dipole", polarization="cross")
p_solver = PathSolver()
paths = p_solver(scene=scene, max_depth=5, los=True, specular_reflection=True,
                 refraction=True, synthetic_array=False, seed=41)
```

**Phase-1 deltas (from RESEARCH Pattern 4, VERIFIED against venv):** `scene = load_scene()` (empty world, no Munich); `scene.frequency = 3.5e9`; `max_depth=0`; `pattern='iso'`; then:
```python
freqs = subcarrier_frequencies(1, 30e3)
H = paths.cfr(frequencies=freqs, normalize=False, out_type='numpy')   # normalize=False is mandatory (Pitfall 4)
pathgain_dB = 10*math.log10(float(np.mean(np.abs(H)**2)))             # ~ -83.36 dB @100m/3.5GHz
```
Then write artifact: HDF5 canonical (`h5py`) + `manifest.json` (schema_version, coord_transform, contract, request_hash, degenerate `sinr_grid` S=1) + `path_gain.bin` (LE float64 `[L]`). Versions to pin in `requirements.txt` from CLAUDE.md: `sionna-rt==2.0.1`, `sionna==2.0.1`, `numpy==2.4.6`, `h5py==3.16.0`.

---

### `tools/sionna_precompute/friis_check.py` (Python test harness, CAL-01)

**Analog (partial):** RESEARCH Pattern 4 verified snippet:
```python
d, lam = 100.0, 3e8/3.5e9
friis_dB = 20*math.log10(lam/(4*math.pi*d))   # -83.32 dB
assert abs(pathgain_dB - friis_dB) < 1.0       # CAL-01 gate; observed residual 0.04 dB
```
Compare at the OMNeT++ Euclidean distance (`phy_->getCoord().distance(coord)` convention, `NrChannelModel.cc:34`) so the gate proves the TOOL-02 transform.

---

### `simulations/nr/sionna/omnetpp.ini` + single-link network (SEAM-01, MOD-01, CAL-02)

**Analog:** `NrNicUe.ned:33,71` parametric-typename wiring — the mechanism the ini drives:
```ned
string nrChannelModelType = default("NrChannelModel_3GPP38_901");   // line 33
nrChannelModel[numNrCarriers]: <nrChannelModelType> like ILteChannelModel {   // line 71
```
LTE equivalent: `LteNicBase.ned:48,101` (`lteChannelModelType` / `channelModel[numCarriers]: <lteChannelModelType>`).

**Ini override (no NED edit):**
```ini
*.*.cellularNic.nrChannelModelType = "SionnaChannelModel"
# Phase-1 inert statistical terms (Pitfall 3):
*.*.cellularNic.nrChannelModel[*].shadowing = false
*.*.cellularNic.nrChannelModel[*].fading = false
*.*.cellularNic.nrChannelModel[*].dynamicLos = false
*.*.cellularNic.nrChannelModel[*].fixedLos = true
```
Reuse an existing single-cell standalone network (RESEARCH suggests `SingleCell_Standalone`). Record a pinned fingerprint baseline in `tests/fingerprint/`.

---

## Shared Patterns

### Fail-loud assertion (`throw cRuntimeError`)
**Source:** `LteRealisticChannelModel.cc:80`, `NrChannelModel.cc:161,183,206,227,297` (used pervasively).
**Apply to:** `SionnaManager` (every contract field) and `ManifestReader` (every malformed-input case).
```cpp
throw cRuntimeError("Unrecognized value in 'fadingType' parameter: \"%s\"", fType.c_str());
```

### INITSTAGE_LOCAL init + base-chaining
**Source:** `LteRealisticChannelModel.cc:43-46`, `NrChannelModel.cc:23-26`.
**Apply to:** `SionnaManager::initialize` and `SionnaChannelModel::initialize` — always call the parent `initialize(stage)` first, gate work on `if (stage == inet::INITSTAGE_LOCAL)`.
```cpp
void NrChannelModel::initialize(int stage) {
    LteRealisticChannelModel::initialize(stage);   // chain first
}
```

### Distance / coord convention (TOOL-02 correctness proof)
**Source:** `NrChannelModel.cc:34`, `LteRealisticChannelModel.cc:106`.
**Apply to:** `SionnaChannelModel::getAttenuation` link lookup and `friis_check.py`.
```cpp
double threeDimDistance = phy_->getCoord().distance(coord);   // inet::Coord, metres
```

### dBm/linear + Define_Module + signal registration
**Source:** `LteRealisticChannelModel.cc:34-41` (`Define_Module`, `registerSignal`), `dBmToLinear` used at `:640,659,1069`.
**Apply to:** `SionnaChannelModel.cc` — `Define_Module(SionnaChannelModel);`; reuse inherited `dBmToLinear`/`linearToDBm` (do not reimplement). License header block: copy verbatim from any analog (`NrChannelModel.h:1-11`).

### Build isolation feature flag
**Source:** `.oppfeatures` `Simu5G_Cars` + root `cppSourceRoots="src" definesFile="src/simu5g/common/features.h"`.
**Apply to:** the new `Simu5G_Sionna` feature; pair `extraSourceFolders` (excludes C++ from default deep build) with `compileFlags="-DWITH_SIONNA"`.

### Offline RT script skeleton
**Source:** `Sionna/rt_step2.py:4-26,33-78` (verified working import + solver shape; auto-selected Mitsuba variant).
**Apply to:** `precompute.py` — never override `mi.variant()`; always `synthetic_array=False`; `normalize=False` on `cfr`.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `SionnaTable.{h,cc}` | model (in-memory store) | transform (lookup) | No existing pure in-memory path-gain table holder in Simu5G; closest related idea is the `std::map<MacNodeId,...>` member maps in `LteRealisticChannelModel.h:90-103`, but those are fading/shadowing caches, not a loaded artifact table. Build fresh: a small struct holding `std::vector<double>` indexed by validated link key; v1 `[L]`, v2-ready `[L,S]`. Planner should use RESEARCH Pattern 2/Architecture diagram, not a codebase analog. |
| `ManifestReader.{h,cc}` (parsing layer) | utility | file-I/O | Only the `std::ifstream` convention (`LteRealisticChannelModel.cc:15`) is analogous; no JSON parser exists in-tree. Use `cValueMap`/`cValueArray` (bundled) or vendored single-header JSON inside the feature dir per RESEARCH. |

## Metadata

**Analog search scope:** `src/simu5g/stack/phy/channelmodel/` (all channel-model classes), `src/simu5g/stack/*.ned` (NIC parametric-typename wiring), `.oppfeatures` (build features), `/home/zoli/Projects/OMNET/Sionna/rt_step{1,2}.py` (offline RT scripts).
**Files scanned:** ~14 (5 channel-model C++/NED, 3 NIC NED, 1 .oppfeatures, 2 Python).
**Pattern extraction date:** 2026-06-17
