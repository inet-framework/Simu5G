# Phase 2: Full BLER Table & CQI/Feedback Consistency - Pattern Map

**Mapped:** 2026-06-18
**Files analyzed:** 7 (5 modified, 2 new)
**Analogs found:** 7 / 7

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `tools/sionna_precompute/precompute.py` | utility (offline producer) | batch transform | self (Phase-1 same file) | exact (extend) |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.{h,cc}` | utility (in-memory table) | CRUD/lookup | self (Phase-1 same file) | exact (extend) |
| `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.{h,cc}` | utility (manifest parse) | file-I/O | self (Phase-1 same file) | exact (extend) |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.{h,cc}` | service (init/assert) | request-response | self (Phase-1 same file) | exact (extend) |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.{h,cc}` | channel model (reception swap) | request-response | `LteRealisticChannelModel::isReceptionSuccessful` | role-match (source swap only) |
| `src/simu5g/stack/phy/channelmodel/sionna/SionnaFeedbackComputation.{h,cc}` | service (CQI feedback) | request-response | `LteFeedbackComputationRealistic` | exact (subclass) |
| `tools/sionna_precompute/tests/test_bler_table.py` | test | batch | `tools/sionna_precompute/tests/test_friis.py` | exact |
| `tests/sionna/unit/test_bler_lookup.cc` | test | CRUD/lookup | `tests/sionna/unit/test_manifest_table.cc` | exact |

---

## Pattern Assignments

### `tools/sionna_precompute/precompute.py` (offline producer, batch transform — extend)

**Analog:** self — `tools/sionna_precompute/precompute.py` (Phase 1)

**Existing structure to follow** (lines 1–40, constants block):
```python
SCHEMA_VERSION = 1
SUBCARRIER_REPRESENTATION = "single-subcarrier-band-center"
_LIB_VERSIONS = { "sionna-rt": "2.0.1", "sionna": "2.0.1", "numpy": "2.4.6", "h5py": "3.16.0" }
```
Phase 2 adds `"torch": "2.12.0"` to `_LIB_VERSIONS` (PHYAbstraction uses torch tensors).

**Validated per-MCS BLER stage — new function, mirrors `compute_path_gains_dB` style** (lines 150–186):
```python
def compute_path_gains_dB(scenario):
    from sionna.rt import (load_scene, PlanarArray, ...)
    validate_scenario(scenario)
    ...
    for tx_xyz, rx_xyz in _link_pairs(scenario):
        ...
        H = paths.cfr(frequencies=freqs, normalize=False, out_type="numpy")
        pathgain_lin = float(np.mean(np.abs(H) ** 2))
        gains.append(10.0 * math.log10(pathgain_lin))
    return gains
```
New `compute_bler_table_dl(eff_sinr_linear_per_link, ...)` follows the same pattern: lazy import, iterate links, collect results into `np.empty((L, len(mcs_list)), dtype="<f8")`. PHYAbstraction call shape (verified in RESEARCH.md Code Examples):
```python
import torch
from sionna.sys import PHYAbstraction

def compute_bler_table_dl(eff_sinr_linear_per_link, mcs_min=2, mcs_max=27,
                          mcs_table_index=2, mcs_category=1):
    phy = PHYAbstraction()   # EESM default; shipped tables; load_bler_tables_from='default'
    L = len(eff_sinr_linear_per_link)
    mcs_list = list(range(mcs_min, mcs_max + 1))
    out = np.empty((L, len(mcs_list)), dtype="<f8")
    num_re = torch.tensor([16800], dtype=torch.int32)
    for li, s in enumerate(eff_sinr_linear_per_link):
        sinr = torch.tensor([s], dtype=torch.float32)
        for mi, mcs in enumerate(mcs_list):
            r = phy(mcs_index=torch.tensor([mcs], dtype=torch.int32),
                    sinr_eff=sinr, num_allocated_re=num_re,
                    mcs_table_index=mcs_table_index, mcs_category=mcs_category,
                    check_mcs_index_validity=False)
            b = float(np.asarray(r[4]).reshape(-1)[0])   # r[4]=bler; NOT r[3] (=tbler)
            if not np.isfinite(b):
                raise ValueError(f"non-finite BLER for link {li} mcs {mcs}: {b}")
            out[li, mi] = b
    return out, mcs_list
```

**Artifact emission pattern** — extend `build_manifest` (lines 207–226) to add new keys:
```python
def build_manifest(scenario, gains_dB):
    return {
        "schema_version": SCHEMA_VERSION,
        ...
        "table_path": "path_gain.bin",
        "table_dtype": "<f8",
        "sinr_grid": [0.0],
        # Phase 2 additions:
        "bler_table_path": "bler.bin",
        "bler_table_dtype": "<f8",
        "mcs_table_index": 2,
        "mcs_category": 1,
        "mcs_min": 2,
        "mcs_max": 27,
        "num_mcs": 26,
        "sinr_effective_fun": "EESM",
        ...
    }
```
Extend `write_artifact` (lines 229–256) to add `bler.bin` write (mirrors existing `path_gain.bin` write):
```python
bler_arr = bler_table.astype("<f8")          # shape [L, num_mcs]
bler_bin_path = os.path.join(out_dir, "bler.bin")
bler_arr.tofile(bler_bin_path)
# also add as HDF5 dataset: h5.create_dataset("bler", data=bler_arr)
```

**Input validation** — extend `validate_scenario` (lines 51–98) with any new SSOT keys; follow existing `_require(cond, msg)` helper pattern.

---

### `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.{h,cc}` (utility, lookup — extend)

**Analog:** self — `SionnaTable.h` (lines 1–60) and `SionnaTable.cc` (lines 1–75)

**Existing member pattern to extend** (`SionnaTable.h` lines 31–56):
```cpp
class SionnaTable {
  protected:
    std::vector<double> pathGainDb_;   // [L]
  public:
    static SionnaTable loadBinary(const std::string& path, std::size_t numLinks);
    double lookup(std::size_t linkIndex) const;
    std::size_t size() const { return pathGainDb_.size(); }
};
```
Add alongside (same protected/public layout):
```cpp
  protected:
    std::vector<double> bler_;         // [L * numMcs_], row-major
    std::size_t numMcs_ = 0;
    int mcsMin_ = 2;
    int mcsMax_ = 27;
    int pinnedMcsTableIndex_ = 2;

  public:
    struct BlerQuery { std::size_t linkId; int mcsTableIndex; int mcsIndex; double effectiveSinr; };
    static void loadBlerBinary(const std::string& path, std::size_t numLinks, std::size_t numMcs,
                               SionnaTable& t);
    double lookupBler(const BlerQuery& q) const;
```

**`loadBinary` pattern** (`SionnaTable.cc` lines 29–65) — exact shape to copy for `loadBlerBinary`:
```cpp
static const std::size_t kMaxLinks = (std::size_t)1 << 24;
// Reject degenerate / oversized declared lengths BEFORE allocating (T-03-01).
if (numLinks == 0)
    throw cRuntimeError("Sionna bler table '%s': declared num_links is 0", path.c_str());
if (numLinks > kMaxLinks)
    throw cRuntimeError("Sionna bler table '%s': declared num_links %lu exceeds cap", ...);
// Validate file size against declared [L * M] * sizeof(double):
const std::streamoff expectedSize = (std::streamoff)(numLinks * numMcs * sizeof(double));
if (fileSize != expectedSize)
    throw cRuntimeError("Sionna bler table '%s': file size %lld != expected %lld", ...);
```

**`lookup` pattern** (`SionnaTable.cc` lines 67–73) — shape to copy for `lookupBler`:
```cpp
double SionnaTable::lookupBler(const BlerQuery& q) const {
    if (q.mcsTableIndex != pinnedMcsTableIndex_)
        throw cRuntimeError("SionnaTable: mcsTableIndex %d != pinned %d", ...);
    if (q.linkId >= numLinks_ || q.mcsIndex < mcsMin_ || q.mcsIndex > mcsMax_)
        throw cRuntimeError("SionnaTable: out-of-range BLER query (link %zu, mcs %d)", ...);
    // v1: effectiveSinr unused (single degenerate S bin)
    return bler_[q.linkId * numMcs_ + (std::size_t)(q.mcsIndex - mcsMin_)];
}
```

---

### `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.{h,cc}` (utility, file-I/O — extend)

**Analog:** self — `ManifestReader.h` (lines 1–63) and `ManifestReader.cc` (lines 1–127)

**`Manifest` struct** to extend (`ManifestReader.h` lines 28–44):
```cpp
struct Manifest {
    int schema_version = 0;
    double carrier_frequency_hz = 0.0;
    double subcarrier_spacing_hz = 0.0;
    int num_bands = 0;
    std::string table_path;
    std::string table_dtype;
    std::size_t num_links = 0;
    std::string coord_transform;
    std::string request_hash;
    // Phase 2 additions:
    std::string bler_table_path;
    int mcs_table_index = 0;
    int mcs_category = 0;
    int mcs_min = 0;
    int mcs_max = 0;
    std::size_t num_mcs = 0;
    std::string sinr_effective_fun;
};
```

**Strict typed extractors** (`ManifestReader.cc` lines 37–93) — reuse exactly, copy call style:
```cpp
// These helpers already exist; call them for the new fields:
m.mcs_table_index    = requireInt(j, "mcs_table_index");
m.mcs_category       = requireInt(j, "mcs_category");
m.mcs_min            = requireInt(j, "mcs_min");
m.mcs_max            = requireInt(j, "mcs_max");
m.num_mcs            = requireSize(j, "num_mcs");
m.bler_table_path    = requireString(j, "bler_table_path");
m.sinr_effective_fun = requireString(j, "sinr_effective_fun");
```
These lines go into `ManifestReader::read` (lines 113–124) following the same per-field pattern.

---

### `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.{h,cc}` (service, init/assert — extend)

**Analog:** self — `SionnaManager.h` (lines 1–74) and `SionnaManager.cc` (lines 1–206)

**`assertContractMatchesLiveScenario`** (`SionnaManager.cc` lines 98–135) — extend with new contract fields:
```cpp
// Existing pattern (lines 102–134): one throw per mismatching field.
// Phase 2: add after existing assertions:
if (m.mcs_table_index != 2)   // pinned value (D-04): must match hardcoded NrMcsTable(extended=true)
    throw cRuntimeError("Sionna manifest mcs_table_index %d != expected 2 (256-QAM extended)", m.mcs_table_index);
if (m.mcs_category != 1)
    throw cRuntimeError("Sionna manifest mcs_category %d != expected 1 (PDSCH/DL)", m.mcs_category);
if (m.num_mcs == 0)
    throw cRuntimeError("Sionna manifest num_mcs is 0 (empty MCS table)");
if (m.sinr_effective_fun.empty())
    throw cRuntimeError("Sionna manifest sinr_effective_fun is empty");
```

**`initialize` stage** (`SionnaManager.cc` lines 137–204) — extend step 4 to load the BLER binary:
```cpp
// After existing table_ = SionnaTable::loadBinary(...) call (line 201-202):
SionnaTable::loadBlerBinary(dirOf(manifestPath) + manifest_.bler_table_path,
                             manifest_.num_links, manifest_.num_mcs, table_);
```
Then add step 5 — D-08 self-consistency check across all links (new, no Phase-1 analog):
```cpp
// D-08: for each link, getCqi at empty-world SINR, assert table BLER <= targetBler_
// Called after the table is loaded. Uses NrAmc + SionnaTable (both available here).
// Pattern: one cRuntimeError per failing link, naming the link index and the BLER found.
runSelfConsistencyCheck(targetBler_);
```

**`SionnaManager.h` `LiveContract`** (lines 40–45) — may stay unchanged; D-08 check reads `targetBler_` from a new NED param or the active `SionnaChannelModel`.

---

### `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.{h,cc}` (channel model, reception swap — MOD-03)

**Analog for the swap:** `LteRealisticChannelModel::isReceptionSuccessful` (`LteRealisticChannelModel.cc` lines 1709–1848)

**Phase-1 `SionnaChannelModel`** (`SionnaChannelModel.cc` lines 1–84) — the override structure to follow. The reception swap does NOT change `SionnaChannelModel.h`; it overrides `isReceptionSuccessful` in `SionnaChannelModel.cc`:

```cpp
// NEW in SionnaChannelModel.cc — override isReceptionSuccessful:
bool SionnaChannelModel::isReceptionSuccessful(LteAirFrame *frame, UserControlInfo *lteInfo)
{
    // 1. Copy header/CQI extraction verbatim from LteRealisticChannelModel.cc:1713-1763
    unsigned char cw = lteInfo->getCw();
    int size = lteInfo->getUserTxParams()->readCqiVector().size();
    if (size == 1) cw = 0;
    Cqi cqi = lteInfo->getUserTxParams()->readCqiVector()[cw];   // line 1726
    if (cqi == 0) return false;                                    // line 1781
    if (cqi > 15) throw cRuntimeError("...cqi:%d...", cqi, ...);
    ...
    // 2. SWAP (MOD-03): replace phyPisaData.getBler (line 1796) with:
    int mcs = cqiToMcsIndex(cqi, DL);                             // D-02 CQI->MCS bridge
    const std::size_t linkIndex = linkKeyFor(nodeId, dir, coord);
    double blockErrorRate = sionnaManager_->getTable().lookupBler(
        {linkIndex, /*mcsTableIndex=*/2, mcs, (double)snrV[band]});
    // 3. Keep unchanged (lines 1801-1838):
    double blockSuccessRate = 1.0 - blockErrorRate;
    double allocationSuccessProbability = pow(blockSuccessRate, (double)allocation);
    cumulativeSuccessProbability *= allocationSuccessProbability;
    ...
    double packetErrorRate = 1.0 - cumulativeSuccessProbability;
    double effectiveErrorRateWithHarq = packetErrorRate * pow(harqReduction_, transmissionAttempt - 1);
    double randomSample = uniform(0.0, 1.0);
    bool receptionFailed = (randomSample <= effectiveErrorRateWithHarq);  // line 1838
    return !receptionFailed;
}
```

**CQI→MCS-index bridge** (Open Q 1 from RESEARCH.md): add a static helper near `NrAmc::getMcsElemPerCqi` (`NrAmc.cc:241-275`). The existing loop `for (unsigned int i = min; i <= max; i++)` in `getMcsElemPerCqi` already finds the matching row; the bridge is the row index `i` at which the loop stopped. Add a sibling function `cqiToMcsIndex(Cqi, Direction)` that returns that `i`.

---

### `src/simu5g/stack/phy/channelmodel/sionna/SionnaFeedbackComputation.{h,cc}` (NEW — CQI feedback, request-response)

**Analog:** `LteFeedbackComputationRealistic.{h,cc}` (lines 1–63 header, lines 83–106 `getCqi`)

**Header pattern** — subclass the existing base (`LteFeedbackComputationRealistic.h` lines 21–60):
```cpp
// SionnaFeedbackComputation.h
#include "simu5g/stack/phy/feedback/LteFeedbackComputationRealistic.h"
#include "simu5g/stack/phy/channelmodel/sionna/SionnaTable.h"

namespace simu5g {

class SionnaFeedbackComputation : public LteFeedbackComputationRealistic
{
    const SionnaTable *table_;       // same table as SionnaChannelModel (shared)
    std::size_t linkId_;
    int mcsTableIndex_;              // pinned to 2 (D-04)
    int mcsMin_;                     // 2 (DL table 2)
    int mcsMax_;                     // 27

  protected:
    Cqi getCqi(TxMode txmode, double snr) override;

  public:
    SionnaFeedbackComputation(Binder *binder, double targetBler, unsigned int numBands,
                              const SionnaTable *table, std::size_t linkId,
                              int mcsTableIndex, int mcsMin, int mcsMax);
};
} //namespace
```

**`getCqi` override** — structure mirrors base `getCqi` (`LteFeedbackComputationRealistic.cc` lines 83–106):
```cpp
// Base getCqi (lines 83-106): iterates CQI 1-15, picks closest-to-targetBler_ by abs diff.
// Sionna override: iterate MCS 2-27, pick highest with BLER <= targetBler_ (exact, D-09).
Cqi SionnaFeedbackComputation::getCqi(TxMode txmode, double snr)
{
    (void)txmode;   // DL only in Phase 2
    int best = -1;
    for (int mcs = mcsMin_; mcs <= mcsMax_; ++mcs) {
        double bler = table_->lookupBler({linkId_, mcsTableIndex_, mcs, snr});
        if (bler <= targetBler_)    // exact comparison (D-09: no epsilon)
            best = mcs;             // highest MCS where BLER <= target
    }
    if (best < 0) return 0;         // no usable MCS (matches base "below quality" → CQI 0, line 87)
    return mcsIndexToCqi(best);     // D-02: MCS-index → CQI via NrAmc (inverse of cqiToMcsIndex)
}
```

**Constructor** — mirrors base constructor (`LteFeedbackComputationRealistic.cc` line 24):
```cpp
// Base: LteFeedbackComputationRealistic(Binder *binder, double targetBler, unsigned int numBands)
//       : targetBler_(targetBler), numBands_(numBands), phyPisaData_(&(binder->phyPisaData))
// Sionna: chain the base, then store extra members.
SionnaFeedbackComputation::SionnaFeedbackComputation(
        Binder *binder, double targetBler, unsigned int numBands,
        const SionnaTable *table, std::size_t linkId, int mcsTableIndex, int mcsMin, int mcsMax)
    : LteFeedbackComputationRealistic(binder, targetBler, numBands),
      table_(table), linkId_(linkId), mcsTableIndex_(mcsTableIndex),
      mcsMin_(mcsMin), mcsMax_(mcsMax)
{}
```

**Factory wiring** (`LtePhyEnb.cc` lines 340–353) — the hardcoded factory must be patched (Pitfall 1):
```cpp
// Current (LtePhyEnb.cc:348):
//   feedbackComputationRealistic_ = new LteFeedbackComputationRealistic(binder_, targetBler, numBands);
// Phase 2: gate on active SionnaManager presence (recommended approach from RESEARCH.md Open Q 2):
SionnaManager *sm = dynamic_cast<SionnaManager *>(
    getModuleByPath(par("sionnaManagerModule").stringValue()));
if (sm != nullptr)
    feedbackComputationRealistic_ = new SionnaFeedbackComputation(
        binder_, targetBler, numBands, &sm->getTable(), linkId, 2, 2, 27);
else
    feedbackComputationRealistic_ = new LteFeedbackComputationRealistic(binder_, targetBler, numBands);
```

---

### `tools/sionna_precompute/tests/test_bler_table.py` (NEW — pytest, batch)

**Analog:** `tools/sionna_precompute/tests/test_friis.py` (lines 1–88, entire file)

**Imports + sys.path pattern** (lines 25–27):
```python
_TOOL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOL_DIR not in sys.path:
    sys.path.insert(0, _TOOL_DIR)
```

**`@pytest.mark.requires_venv` pattern** (line 60):
```python
@pytest.mark.requires_venv
def test_bler_table_monotone():
    from precompute import compute_bler_table_dl
    ...
```

**Scenario fixture** — reuse `_SCENARIO` dict shape (lines 37–57); add any new SSOT keys. Tests to cover per RESEARCH.md Validation Architecture:
- BLER[L,M] monotone in MCS (for the single empty-world SINR, BLER generally increases with MCS; at ~70 dB expect near-0 for most).
- Determinism: call `compute_bler_table_dl` twice, assert identical.
- Finite for MCS 2–27: assert `np.all(np.isfinite(bler))`.
- `inf` rejected for MCS 0/1: assert the tool raises `ValueError` when category gives non-finite.
- MCS 27 has BLER floor > 0.01 at 70 dB: `assert bler[0, 25] > 0` (MCS 27 = index 25 in 2..27).

---

### `tests/sionna/unit/test_bler_lookup.cc` (NEW — C++ standalone harness)

**Analog:** `tests/sionna/unit/test_manifest_table.cc` (lines 1–80+)

**Harness skeleton** (lines 1–62 of `test_manifest_table.cc`):
```cpp
#include <omnetpp.h>
#include "simu5g/stack/phy/channelmodel/sionna/SionnaTable.h"
using namespace omnetpp;
using namespace simu5g;

static int g_failures = 0, g_checks = 0;
#define CHECK(cond, msg) do { ++g_checks; if (!(cond)) { ++g_failures; fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__); } } while(0)

template<typename F>
static bool throwsRuntimeError(F fn) { try { fn(); } catch (const cRuntimeError&) { return true; } catch(...) {} return false; }
```

**Binary fixture helper** (lines 73–80 of `test_manifest_table.cc`):
```cpp
static std::string writeBinaryDoubles(const std::string& name, const std::vector<double>& vals) {
    std::string path = std::string("/tmp/") + name;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(vals.data()), vals.size() * sizeof(double));
    return path;
}
```
Extend for 2D `[L, M]` BLER: write `L * M` doubles, pass `numLinks=L`, `numMcs=M` to `loadBlerBinary`.

**Test cases to cover** (per RESEARCH.md Validation Architecture):
- `lookupBler` valid key returns stored value.
- `lookupBler` bad `linkId` → `cRuntimeError`.
- `lookupBler` bad `mcsIndex < mcsMin_` → `cRuntimeError`.
- `lookupBler` bad `mcsIndex > mcsMax_` → `cRuntimeError`.
- `lookupBler` wrong `mcsTableIndex` → `cRuntimeError`.
- `cqiToMcsIndex` / `mcsIndexToCqi` round-trip for CQI 1–15.
- D-08 pure logic: assert `table.lookupBler(link, mcs) <= targetBler_` for the feedback-selected MCS.

---

## Shared Patterns

### `cRuntimeError` fail-loud (CAL-02)
**Source:** `SionnaManager.cc` lines 98–134; `SionnaTable.cc` lines 32–62; `ManifestReader.cc` lines 42–93
**Apply to:** ALL new/extended C++ files (`SionnaTable`, `SionnaManager`, `SionnaFeedbackComputation`, `SionnaChannelModel` reception override, `LtePhyEnb` factory patch)
```cpp
throw cRuntimeError("Sionna <component>: <what was wrong> (field '%s', value %...)", ...);
```
Never silently fall back; one throw per mismatch; message names the component and the offending value.

### Strict typed manifest extractors (`requireInt`, `requireSize`, `requireDouble`, `requireString`)
**Source:** `ManifestReader.cc` lines 37–93
**Apply to:** `ManifestReader::read` extension (Phase 2 new manifest fields)
All new fields use these helpers; no `j["key"].get<T>()` inline — always through the strict extractor.

### Binary file load validation (size-before-read)
**Source:** `SionnaTable::loadBinary` (`SionnaTable.cc` lines 29–65)
**Apply to:** `SionnaTable::loadBlerBinary`
Pattern: `(1)` reject zero/oversized declared length, `(2)` open file `ios::ate`, `(3)` compute `expectedSize = numLinks * numMcs * sizeof(double)`, `(4)` assert `fileSize == expectedSize`, `(5)` seekg(0), read, `(6)` assert `gcount == expectedSize`.

### `@pytest.mark.requires_venv` + `sys.path` bootstrap
**Source:** `tests/test_friis.py` lines 25–27, 60
**Apply to:** `tests/test_bler_table.py` (all test functions that import from `precompute`)

### `throwsRuntimeError<F>` template
**Source:** `tests/sionna/unit/test_manifest_table.cc` lines 49–62
**Apply to:** `tests/sionna/unit/test_bler_lookup.cc` — copy verbatim, reuse for all bounds-check assertions.

### Degenerate S-axis comment
**Source:** `precompute.py` lines 221–224 (`"sinr_grid": [0.0]` comment)
**Apply to:** Phase 2 additions in `build_manifest` and `SionnaTable` internal comments — always note "v1 degenerate S=1; v2 adds real SINR-bin interpolation here" so the forward-compatibility intent is documented at each extension point.

---

## No Analog Found

All files have analogs within the codebase.

| File | Note |
|------|------|
| `SionnaFeedbackComputation` | Closest analog is `LteFeedbackComputationRealistic` — exact subclass relationship; no prior Sionna-specific feedback class exists. Treat `getCqi` override as a fresh implementation following the base's structure. |

---

## Key Anti-Patterns (from RESEARCH.md)

| Anti-Pattern | Where It Bites | Safe Alternative |
|---|---|---|
| `mcs_category=0` for DL | `compute_bler_table_dl` | Always `mcs_category=1` (PDSCH). `0` is PUSCH/UL. |
| `float(np.asarray(r[4]))` on 1-D array | PHYAbstraction output extraction | `float(np.asarray(r[4]).reshape(-1)[0])` — bare `float()` on 1-D raises `TypeError`. |
| Storing `inf` BLER for MCS 0/1 | tool writer + C++ loader | Reject at tool write time (`np.isfinite` check); also reject non-finite at `loadBlerBinary`. |
| Selecting `SionnaFeedbackComputation` via ini typename | `LtePhyEnb` factory | Gate on `SionnaManager` presence in `initializeFeedbackComputation` (hardcoded factory, not NED class). |
| Two different CQI↔MCS mappings in reception vs feedback | `cqiToMcsIndex` bridge | One shared `cqiToMcsIndex(Cqi, Direction)` helper — both readers call it identically (D-02). |
| `epsilon` in `BLER <= targetBler_` comparison | `getCqi` + D-08 self-check | Exact `<=` on the identical stored `double` (D-09). |
| Dropping `harqReduction_` multiply | Reception swap in `isReceptionSuccessful` | Copy `effectiveErrorRateWithHarq = per * pow(harqReduction_, transmissionAttempt - 1)` verbatim (line 1817). |

---

## Metadata

**Analog search scope:** `src/simu5g/stack/phy/channelmodel/sionna/`, `src/simu5g/stack/phy/feedback/`, `src/simu5g/stack/mac/amc/`, `tools/sionna_precompute/`, `tests/sionna/unit/`
**Files scanned:** 14 source files read directly
**Pattern extraction date:** 2026-06-18
