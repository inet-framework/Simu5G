---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
reviewed: 2026-06-17T00:00:00Z
depth: standard
files_reviewed: 18
files_reviewed_list:
  - src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc
  - src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.h
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.h
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.h
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.ned
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.cc
  - src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.h
  - tests/sionna/check_default_build_symbols.sh
  - tests/sionna/unit/run_unit_tests.sh
  - tests/sionna/unit/test_contract_assertion.cc
  - tests/sionna/unit/test_manifest_table.cc
  - tools/sionna_precompute/friis_check.py
  - tools/sionna_precompute/precompute.py
  - tools/sionna_precompute/tests/test_friis.py
  - tools/sionna_precompute/scenario.example.json
  - simulations/NR/sionna/omnetpp.ini
findings:
  critical: 1
  warning: 7
  info: 6
  total: 14
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-06-17
**Depth:** standard
**Files Reviewed:** 18
**Status:** issues_found

## Summary

This phase implements an opt-in Sionna channel model: a C++ reader half (ManifestReader,
SionnaTable, SionnaManager, SionnaChannelModel) plus a Python precompute half. The core
correctness anchors the review targeted are mostly sound:

- The dB sign convention is **correct**: `getSINR` does `recvPower -= attenuation`, and
  `SionnaChannelModel::getAttenuation` returns `-pathGain_dB`, turning a negative Sionna
  path gain into a positive loss (verified against `LteRealisticChannelModel.cc:514`).
- The binary table loader bounds-checks file size against declared `num_links`, caps
  `num_links` before allocation, and rejects short reads — solid input validation.
- The manifest contract assertion checks every contract field and fails loud.
- The SEAM-02 default-build symbol gate is carefully constructed.

The most serious defect is a **silent contract gap**: the manifest carries
`carrier_frequency_hz` / `subcarrier_spacing_hz` / `num_bands`, the contract assertion
compares them against `SionnaManager`'s *own* NED parameters, but **nothing forces those
NED parameters to equal the live Simu5G carrier/SCS/numBands actually used by the PHY**.
The ini sets them by hand in two independent places; a divergence passes every assertion
yet silently runs a mismatched channel — exactly the failure the parameter contract exists
to prevent. Several robustness and consistency warnings follow.

## Critical Issues

### CR-01: Parameter contract validates against self-declared NED params, not the live PHY — a real mismatch can pass silently

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:112-116`, `SionnaManager.ned:28-30`, `simulations/NR/sionna/omnetpp.ini:49-55,77`
**Issue:**
The whole point of the contract (CLAUDE.md: "carrier frequency, bandwidth, numerology/SCS,
band count ... must be identical on both sides; mismatch must fail loudly at init") is to
guarantee the artifact matches the simulation. But `assertContractMatchesLiveScenario`
compares the manifest only against `SionnaManager`'s own parameters:

```cpp
live.carrier_frequency_hz = par("carrierFrequencyHz").doubleValue();
live.subcarrier_spacing_hz = par("subcarrierSpacingHz").doubleValue();
live.num_bands = par("numBands").intValue();
```

These `SionnaManager` parameters are set independently of the actual carrier configured on
`carrierAggregation.componentCarrier[0]`. In `omnetpp.ini` the real carrier is set at
lines 53-55 (`carrierFrequency = 3.5GHz`, `numerologyIndex = 1`, `numBands = 1`), while the
`SionnaManager` contract values are **never set in this ini at all** — `carrierFrequencyHz`,
`subcarrierSpacingHz`, and `numBands` have no `default()` in the NED and no assignment in
`omnetpp.ini`. As written this configuration **cannot even initialize** (unassigned
mandatory NED params abort), and once a user does assign them, they can trivially set the
manager's `carrierFrequencyHz = 3.5e9` while the actual `componentCarrier[0].carrierFrequency`
is, say, 2.6 GHz. The contract passes; the run is silently wrong. The artifact is validated
against a hand-copied shadow of the live values, not the live values themselves.

**Fix:** Resolve the live carrier/SCS/numBands from the actual carrier-aggregation /
component-carrier module (the same source the PHY uses) rather than from standalone
manager parameters. At minimum, read them from the real `componentCarrier` submodule and
assert against those. If standalone params are kept as a convenience, additionally assert
they equal the live component-carrier values so a hand-copy divergence aborts:

```cpp
// pseudo: pull the authoritative values the PHY actually uses
cModule *cc = getModuleByPath(par("componentCarrierModule"));
double liveFc = cc->par("carrierFrequency").doubleValue();
// numerologyIndex -> SCS = 15kHz * 2^mu
int mu = cc->par("numerologyIndex").intValue();
live.subcarrier_spacing_hz = 15000.0 * (1 << mu);
live.num_bands = cc->par("numBands").intValue();
// then assert manifest == live (already done), guaranteeing artifact == real sim config
```

Until the contract binds to the values the PHY genuinely consumes, the "fail-loud parameter
contract" guarantee in CLAUDE.md is not actually met.

## Warnings

### WR-01: `linkKeyFor` always returns 0 — interferer attenuation would be silently misattributed if downlink interference is ever enabled

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc:40-47,63-64`
**Issue:**
`getAttenuation` is called by `getSINR` for the *serving* link, but in the base model it is
also reachable for interfering cells, and `getAttenuation` is the public override point. With
`linkKeyFor` hard-wired to index 0, every link — serving or interfering — receives link 0's
path gain. In v1 `downlinkInterference` defaults to `false` and is not enabled in the ini, so
this is latent rather than active, but it is a correctness landmine: the moment a second cell
or interference is introduced, the model returns the wrong gain with no error. There is no
guard asserting `num_links == 1` (the only case for which index-0-always is correct), so the
invariant the code depends on is undocumented and unchecked.

**Fix:** Add a hard assertion in `initialize` that the table size matches the single-link
assumption for v1 (e.g. `if (sionnaManager_->getTable().size() != 1) throw cRuntimeError(...)`),
or refuse to run multi-cell scenarios until `linkKeyFor` maps (Tx,Rx) properly. Fail loud
rather than silently returning link 0 for an unmodeled link.

### WR-02: `requireInt` accepts unsigned JSON values that overflow `int`

**File:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc:44-51`
**Issue:**
`requireInt` accepts `is_number_unsigned()` then calls `v.get<int>()`. For a manifest value
like `num_bands: 4294967296`, nlohmann's `get<int>()` performs an out-of-range narrowing
conversion (implementation-defined / UB-adjacent), silently yielding a garbage `num_bands`.
The validation claims to reject "type confusion" but does not reject range overflow. Since
`num_bands` feeds the contract assertion, a corrupt manifest could produce a nonsensical
value that still compares unequal-but-unpredictably.

**Fix:** Range-check before narrowing:
```cpp
long long raw = v.get<long long>();
if (raw < INT_MIN || raw > INT_MAX)
    throw cRuntimeError("Sionna manifest field '%s' out of int range", key);
return (int)raw;
```

### WR-03: `num_links` from manifest is fully trusted as the binary table length with no independent corroboration

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:120-121`, `SionnaTable.cc:49-54`
**Issue:**
`loadBinary` validates that `fileSize == num_links * sizeof(double)`, which is good against
truncation. But `num_links` itself comes only from the JSON manifest; if the manifest and the
`.bin` are independently regenerated/edited, a manifest declaring `num_links: 2` with a 16-byte
file passes the size check while silently describing the wrong table shape. The manifest also
carries no per-link identity, so there is no way to detect that the table's row 0 corresponds
to the intended Tx/Rx pair. The `request_hash` is asserted non-empty but never recomputed or
compared, so it provides zero integrity in the C++ side despite the "integrity key" comments.

**Fix:** At minimum, document that `request_hash` is decorative on the C++ side (it currently
implies more than it delivers). Better: have the C++ side recompute or at least compare the
table's expected length against an HDF5/manifest cross-field, and in v2 bind link rows to
(Tx,Rx) identity so a reshaped table fails loud.

### WR-04: Exact floating-point equality for the carrier/SCS contract is brittle across JSON/NED round-trips

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:82-88`
**Issue:**
`m.carrier_frequency_hz != live.carrier_frequency_hz` uses exact `!=` on doubles. The manifest
value is parsed from JSON text (`3.5e9`) while the live value comes from NED unit parsing
(`3.5GHz` -> `3.5e9`). These happen to coincide for round binary-representable values, but any
value that is not exactly representable (or that travels through `GHz`/`kHz` unit conversion
with a different intermediate) will compare unequal and abort a legitimately-matching run, or
(less likely) mask a small intended difference. The same applies to `subcarrier_spacing_hz`.

**Fix:** Compare with a relative tolerance appropriate to the contract (e.g.
`fabs(a-b) <= 1e-6 * fabs(b)`), or normalize both sides to integer Hz before comparing. Keep
`num_bands` / `schema_version` as exact integer compares.

### WR-05: `isIdentityTransform` accepts a wrong-length or scale-missing origin as "identity"

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc:42-70`
**Issue:**
The identity check is permissive in ways that can let a non-identity transform pass:
- If `origin` is present but **not** an array (e.g. a scalar or object), the `is_array()` branch
  is skipped entirely and the origin is never validated — a malformed origin is treated as
  identity.
- If `origin` is an array of length 0, 1, or 2 (not 3), it still passes as long as the present
  elements are 0.
- If `scale` is missing, identity is assumed; if `scale` is present but **not** a number, the
  check is skipped and a non-numeric scale passes.
- `units` and `handedness` (which the precompute tool and example both populate, e.g.
  `units:"m"`, `handedness:"right"`) are not checked at all, so a manifest claiming
  `units:"km"` or `handedness:"left"` with `axis_map:"identity"` passes as identity while the
  geometry is actually scaled/mirrored.

Given Phase 1's hard requirement that the OMNeT++ and Sionna frames coincide so the Euclidean
distance is the Friis reference, this is a real correctness hole.

**Fix:** Make the check strict and positive: require `axis_map=="identity"`, `scale` present
and `==1.0`, `origin` present, an array of exactly 3, all `0.0`, `units=="m"`, and
`handedness=="right"`. Reject anything missing or of the wrong type rather than defaulting to
"identity".

### WR-06: SEAM-02 symbol gate can pass on the wrong artifact (a Sionna-enabled build) without detecting it

**File:** `tests/sionna/check_default_build_symbols.sh:60-100,141-153`
**Issue:**
The gate's job is to prove the *default (feature OFF)* build contains no Sionna symbols. But
nothing ties the inspected artifact to a feature-OFF build: the script globs whatever
`libsimu5g*.so` it finds under `src/`/`out/`. If the developer last built with
`Simu5G_Sionna` ON, the script inspects that ON library — which *does* contain `Sionna*`
symbols — and reports FAIL, or, on a stale tree, inspects an old OFF library and reports PASS
while the current tree is ON. The PASS/FAIL is decoupled from the build configuration it
claims to certify. There is no assertion that the feature was actually OFF when the inspected
object was produced.

**Fix:** Have the gate build the default target itself (feature explicitly OFF) into a known
output path and inspect exactly that, or require the caller to pass the known-OFF artifact and
record the feature state. As-is, the gate's guarantee is only as good as the developer's
memory of how they last built.

### WR-07: `friis_check.py` / `test_friis` compare against Friis at the 3D Euclidean distance but the harness comment ties it to a "horizontal 100 m reference"

**File:** `tools/sionna_precompute/tests/test_friis.py:62-79`, `tools/sionna_precompute/friis_check.py:27-47`
**Issue:**
The Tx is at z=10, Rx at z=1.5, x=100 — so the true 3D distance is ~100.36 m, and both the
test and `friis_check` correctly compute `d` as the 3D Euclidean distance and feed *that* into
`friis_dB`. However the test's secondary assertion comment (line 75-76) says "Use the
horizontal 100 m reference for the band," which is inconsistent with the 3D distance actually
used and could mislead a maintainer into "fixing" the distance to the horizontal 100 m,
breaking the (correct) round-trip. The 1.0 dB tolerance is also loose enough that a future
sign or unit slip of up to ~1 dB would pass undetected.

**Fix:** Correct the misleading comment to state the 3D Euclidean distance is used (matching
`phy_->getCoord().distance(coord)`), and consider tightening the tolerance toward the observed
~0.04 dB residual (e.g. 0.25 dB) so a real convention slip is caught.

## Info

### IN-01: `ManifestReader.h` / `SionnaTable.h` doc comments still describe the classes as "compilable skeleton ... filled in by Plan 01-03"

**File:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.h:26-27`, `SionnaTable.h:27-28`, `SionnaChannelModel.h:28`, `SionnaManager.h:31-32`
**Issue:** The bodies are now implemented, but the headers still say they are skeletons to be
filled in later. Stale comments mislead future readers about implementation status.
**Fix:** Remove the "compilable skeleton / filled in by Plan 01-03" notes now that the code is
implemented.

### IN-02: `getAttenuation` computes `threeDimDistance` only to discard it

**File:** `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.cc:57-58`
**Issue:** `threeDimDistance` is computed then immediately `(void)`-cast away. The comment
explains the intent (keep the funnel identical for future validation), but as committed it is
dead computation that a reader will flag.
**Fix:** Either drop it, or wire it into an `EV`/assert that the live distance matches the
artifact's link distance (which would also strengthen WR-03's identity gap).

### IN-03: `requireSize` reads negative-rejected value via `get<unsigned long long>()` redundantly

**File:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc:53-59`
**Issue:** The guard accepts either `is_number_unsigned()` or a non-negative signed integer,
then extracts via `get<unsigned long long>()`. The logic is correct but the two-branch
predicate plus a third extraction type is harder to read than necessary and does not range-cap
against `kMaxLinks` here (the cap lives only in `SionnaTable`).
**Fix:** Simplify to a single non-negative check and document that the upper bound is enforced
downstream in `SionnaTable::loadBinary`.

### IN-04: Manifest fields `table_dtype`, `num_bands` width, and `subcarrier_representation` parsed/declared but never enforced

**File:** `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.cc:113`, `precompute.py:221`
**Issue:** The manifest declares `table_dtype: "<f8"` and the producer writes little-endian
float64, but the C++ loader reads raw `double` with no check that `table_dtype == "<f8"` nor
any endianness handling. On a big-endian host the table would be silently byte-swapped. v1
targets little-endian only, but the declared dtype is collected and ignored.
**Fix:** Assert `m.table_dtype == "<f8"` before `loadBinary`, making the (currently implicit)
LE/float64 assumption explicit and fail-loud.

### IN-05: `compute_request_hash` hashes the full scenario but the C++ side never verifies it — comment overstates its role

**File:** `tools/sionna_precompute/precompute.py:192-200`
**Issue:** The hash is described as a "cache/integrity key"; the C++ side only checks it is
non-empty. This is consistent with the "not a security control" caveat, but readers may assume
integrity is enforced. (Cross-ref WR-03.)
**Fix:** Note in the manifest/header that `request_hash` is producer-side provenance only and
is not validated for integrity by the consumer.

### IN-06: `precompute.py` reloads/rebuilds the scene per link and calls `validate_scenario` twice

**File:** `tools/sionna_precompute/precompute.py:146-147,162,169-186`
**Issue:** `compute_path_gain_dB` calls `compute_path_gains_dB`, which calls
`validate_scenario` again even though `load_scenario` already validated. Inside the loop a
fresh `load_scene()` + arrays are rebuilt per link. Functionally correct for v1's single link;
flagged only as redundant work / double validation, not a defect. (Performance is out of scope;
noted for the duplicate-validation code smell.)
**Fix:** Validate once at the entry point and hoist scene/array setup out of the per-link loop
when the geometry-independent parts are shared.

---

_Reviewed: 2026-06-17_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
