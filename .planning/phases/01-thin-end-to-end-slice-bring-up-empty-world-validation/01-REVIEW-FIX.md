---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
fixed_at: 2026-06-18T14:10:00Z
review_path: .planning/phases/01-thin-end-to-end-slice-bring-up-empty-world-validation/01-REVIEW.md
iteration: 1
findings_in_scope: 10
fixed: 10
skipped: 0
status: all_fixed
---

# Phase 1: Code Review Fix Report

**Fixed at:** 2026-06-18
**Source review:** `.planning/phases/01-thin-end-to-end-slice-bring-up-empty-world-validation/01-REVIEW.md`
**Iteration:** 1

**Summary:**
- Findings in scope: 10 (CR-01, WR-01–WR-07, IN-01, IN-04)
- Fixed: 10
- Skipped: 0

## Build / Test Results

| Check | Result |
|-------|--------|
| `cd src && make MODE=debug -j$(nproc)` | Exit 0 — only the 4 modified Sionna .cc files recompiled |
| Unit tests `tests/sionna/unit/run_unit_tests.sh` | **27/27 PASS** (20/20 manifest+table, 7/7 contract) |
| Shipped sim `rcvdSinrDl:mean` | **70.14 dB** — unchanged (CAL-01 preserved) |
| Committed `manifest.json` under strict WR-05 | Passes — all 5 fields present with correct values |
| `pytest tests/test_friis.py` (venv) | **1/1 PASS** at new 0.25 dB tolerance (residual ~0.04 dB) |

## Fixed Issues

### CR-01: Parameter contract validates against self-declared NED params, not the live PHY

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.ned`,
  `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc`,
  `simulations/NR/sionna/omnetpp.ini`
**Commit:** `92ddad11`
**Applied fix:** Added optional NED string param `componentCarrierModule = default("")`.
When non-empty, `SionnaManager::initialize` resolves that module via `getModuleByPath`,
reads `carrierFrequency` (→ Hz), `numerologyIndex` (→ SCS = 15000 * 2^mu), and `numBands`
from the live ComponentCarrier, then cross-checks them against the shadow params using the
same 1 ppm tolerance as WR-04. A divergence throws `cRuntimeError` loud. When the param
is empty (default), behaviour is completely unchanged — no new requirement on existing users.
The shipped example ini sets `componentCarrierModule = "SionnaSingleLink.carrierAggregation.componentCarrier[0]"`.
The run still initializes and produces `rcvdSinrDl:mean = 70.14 dB` (values are consistent).
Added `// TODO(phase-2): bind directly without shadow params` comment.

### WR-01: `linkKeyFor` always returns 0 — no guard against multi-link tables

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc`
**Commit:** `f8900691`
**Applied fix:** After acquiring `sionnaManager_` in `initialize`, check
`sionnaManager_->getTable().size() != 1` and throw `cRuntimeError` if the table has
more than one link. This makes the v1 single-link assumption explicit and fail-loud;
the comment notes that v2 will remove this guard when `linkKeyFor` maps (Tx,Rx) properly.

### WR-02: `requireInt` accepts unsigned JSON values that overflow `int`

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc`
**Commit:** `56c207ad`
**Applied fix:** Extract via `v.get<long long>()` first, then range-check against
`INT_MIN`/`INT_MAX`, throw `cRuntimeError` if out of range, then cast to `int`. Added
`#include <climits>` for the constants.

### WR-03: `request_hash` comment overstates consumer-side integrity enforcement

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc`
**Commit:** `b0e3ff3e` (combined with WR-04)
**Applied fix:** Updated the comment block around the `request_hash` check to state
explicitly that this is producer-side provenance only; the C++ consumer does not
recompute or verify integrity — it only rejects a missing/empty field. Also updated
`ManifestReader.h` field comments in the IN-01 commit.

### WR-04: Exact float equality for carrier/SCS contract is brittle

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc`
**Commit:** `b0e3ff3e`
**Applied fix:** Replaced `!=` comparisons for `carrier_frequency_hz` and
`subcarrier_spacing_hz` with `std::fabs(a-b) > 1e-6 * std::fabs(b)` (1 ppm relative
tolerance). Added `#include <cmath>`. `num_bands` and `schema_version` remain exact
integer compares. The same tolerance is applied to the new CR-01 cross-check.

### WR-05: `isIdentityTransform` accepts missing/malformed fields as identity

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc`
**Commit:** `37a8f113`
**Applied fix:** Rewrote `isIdentityTransform` to require ALL five fields present
with correct types and values: `axis_map == "identity"`, `scale` present as a number
`== 1.0`, `origin` present as an array of exactly 3 numbers all `== 0.0`, `units == "m"`,
`handedness == "right"`. Any missing, wrong-type, or wrong-value field returns false.
The committed `manifest.json` and both unit-test manifests already carry all five fields
and continue to pass the strict check.

### WR-06: SEAM-02 symbol gate can pass on a Sionna-enabled library

**Files modified:** `tests/sionna/check_default_build_symbols.sh`
**Commit:** `c979b904`
**Applied fix:** Added three operating modes:
- `--build`: compiles a fresh feature-OFF library via `opp_featuretool disable` + `make`,
  inspects that artifact, restores feature state. Self-certifying regardless of current tree.
- `<explicit-path>`: unchanged behaviour.
- `(no args)`: same glob probe as before, but when `opp_featuretool list -e` reports
  `Simu5G_Sionna` is currently ENABLED, exit 3 (AMBIGUOUS) instead of potentially
  certifying a stale feature-OFF `.so`. Exit code 3 added with clear diagnostic.
The forbidden-symbol set is unchanged.

### WR-07: Friis test has misleading "horizontal 100 m" comment; tolerance is loose

**Files modified:** `tools/sionna_precompute/tests/test_friis.py`,
  `tools/sionna_precompute/friis_check.py`
**Commit:** `d95410cc`
**Applied fix:** Corrected module docstring and secondary assert comment to state the
3D Euclidean distance (~100.36 m) is used — matching `phy_->getCoord().distance(coord)`.
Tightened the tolerance from 1.0 dB to 0.25 dB; the observed residual is ~0.04 dB so
this passes cleanly. Verified: `pytest tests/test_friis.py` PASS under the offline venv.
Also clarified `_primary_link_distance` docstring in `friis_check.py`.

### IN-01: Stale "compilable skeleton / filled in by Plan 01-03" comments

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.h`,
  `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.h`,
  `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h`,
  `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.h`
**Commit:** `0848c444`
**Applied fix:** Removed all "compilable skeleton / filled in by Plan 01-03" lines from
the four headers. Updated `coord_transform` and `request_hash` field comments in
`ManifestReader.h` to reflect the current semantics.

### IN-04: `table_dtype` declared but never enforced before `loadBinary`

**Files modified:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc`
**Commit:** `8bff97bf`
**Applied fix:** Added an explicit check `if (manifest_.table_dtype != "<f8")` before
calling `SionnaTable::loadBinary`, throwing `cRuntimeError` for any other dtype. Makes
the implicit little-endian float64 assumption explicit and fail-loud (a big-endian or
integer-typed artifact is rejected rather than silently misread).

## Skipped Issues

None — all 10 in-scope findings were applied.

---

_Fixed: 2026-06-18_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
