---
phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation
plan: 02
subsystem: infra
tags: [oppfeatures, build-isolation, channelmodel, omnetpp, ned, cpp, sionna, seam]

# Dependency graph
requires:
  - phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation (Plan 01-01)
    provides: empty-world Friis-vs-Sionna path-gain validation + coord_transform/dB convention (parallel; no shared files)
provides:
  - Simu5G_Sionna .oppfeatures feature (initiallyEnabled=false) isolating the sionna source folder from the default deep build (SEAM-02)
  - SEAM-02 symbol-check gate (tests/sionna/check_default_build_symbols.sh)
  - Compilable C++ skeleton: SionnaChannelModel, SionnaManager (NED-registered) + SionnaTable, ManifestReader (plain utilities)
  - SionnaChannelModel.ned extends NrChannelModel @class (SEAM-01 registration)
affects: [01-03 (fills in getAttenuation override, artifact load + contract assertion, binary/JSON loaders), 01-04 (executes the SEAM-02 gate against a real feature-OFF build)]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Build-isolation feature flag: extraSourceFolders excludes a source folder from the default deep build; opt-in via Simu5G_Sionna"
    - "NED-registered channel-model subclass: simple X extends NrChannelModel @class(X) satisfies ILteChannelModel via the parent chain (no NED-interface edit)"

key-files:
  created:
    - .oppfeatures (modified - added Simu5G_Sionna feature)
    - tests/sionna/check_default_build_symbols.sh
    - src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.{h,cc,ned}
    - src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.{h,cc,ned}
    - src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.{h,cc}
    - src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.{h,cc}
  modified:
    - .oppfeatures

key-decisions:
  - "Simu5G_Sionna feature pairs extraSourceFolders (excludes the C++ from the default .so) with compileFlags=-DWITH_SIONNA, mirroring Simu5G_Cars structure but with a non-empty extraSourceFolders"
  - "SionnaChannelModel subclasses NrChannelModel (not LteChannelModel directly), inheriting the NR distance/getAttenuation funnel; getAttenuation stub delegates to the base until Plan 01-03 swaps in the table lookup"
  - "SionnaManager owns the Manifest + SionnaTable members and is the single artifact-load + contract-assertion point; SionnaChannelModel acquires the loaded table from it"

patterns-established:
  - "Build isolation: a disabled .oppfeatures feature with extraSourceFolders is the mechanism that keeps the default build byte-for-byte unaffected"
  - "Skeleton-first: NED-registered modules + plain utilities are landed as compilable stubs with TODO(01-03) markers before any real Sionna logic"

requirements-completed: [SEAM-02]

# Metrics
duration: 3min
completed: 2026-06-17
---

# Phase 1 Plan 02: Sionna build-isolation seam + C++ skeleton Summary

**Simu5G_Sionna .oppfeatures feature (initiallyEnabled=false) excludes the new sionna/ source folder from the default deep build, plus a SEAM-02 symbol-check gate and the four compilable C++ class-pairs (SionnaChannelModel, SionnaManager, SionnaTable, ManifestReader) the real logic plugs into.**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-06-17T18:51:30Z
- **Completed:** 2026-06-17T18:54:08Z
- **Tasks:** 3
- **Files modified:** 11 (.oppfeatures + 1 script + 9 C++/NED skeleton files)

## Accomplishments
- Added the `Simu5G_Sionna` build-isolation feature (`initiallyEnabled="false"`, `extraSourceFolders="src/simu5g/stack/phy/channelmodel/sionna"`, `compileFlags="-DWITH_SIONNA"`) — the load-bearing SEAM-02 mechanism keeping the default build unaffected.
- Created `tests/sionna/check_default_build_symbols.sh`: robust binary-path resolution + `nm -D`/`nm` fallback + `grep -iE 'sionna|hdf5|python|tensorflow|torch'`, exiting nonzero on any match. It is the SEAM-02 gate (executed against a real feature-OFF build by Plan 01-04).
- Created two NED-registered modules: `SionnaChannelModel` (`: public NrChannelModel`, `@class`, `getAttenuation` override stub delegating to the base) and `SionnaManager` (`cSimpleModule`, `initialize` gated on `inet::INITSTAGE_LOCAL`, holds `Manifest` + `SionnaTable`).
- Created two plain utilities: `SionnaTable` (`std::vector<double> pathGainDb_`, `loadBinary`/`lookup` stubs, no `Define_Module`) and `ManifestReader` (`struct Manifest` with the full contract field set + `static read` stub). No HDF5/Python headers pulled in.

## Task Commits

Each task was committed atomically:

1. **Task 1: Simu5G_Sionna build-isolation feature + SEAM-02 symbol gate** - `964091de` (feat)
2. **Task 2a: NED-registered skeleton classes (SionnaChannelModel + SionnaManager)** - `7b2fbd52` (feat)
3. **Task 2b: Plain utility skeleton classes (SionnaTable + ManifestReader)** - `d8fa2462` (feat)

**Plan metadata:** (this commit) `docs(01-02): complete plan`

## Files Created/Modified
- `.oppfeatures` - Added the `Simu5G_Sionna` feature element (excludes the sionna/ folder from the default deep build).
- `tests/sionna/check_default_build_symbols.sh` - SEAM-02 gate: asserts the default binary links zero sionna/hdf5/python/tensorflow/torch symbols.
- `src/simu5g/stack/phy/channelmodel/sionna/SionnaChannelModel.{h,cc,ned}` - Opt-in channel model subclassing `NrChannelModel`; `getAttenuation` override stub (01-03 swaps in `-table_->lookup(...)`).
- `src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.{h,cc,ned}` - `cSimpleModule` artifact loader + contract-assertion skeleton; `initialize` gated on `inet::INITSTAGE_LOCAL`.
- `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.{h,cc}` - Plain path-gain table holder (`std::vector<double>`), `loadBinary`/`lookup` stubs.
- `src/simu5g/stack/phy/channelmodel/sionna/ManifestReader.{h,cc}` - `struct Manifest` (schema_version, carrier_frequency_hz, subcarrier_spacing_hz, num_bands, table_path, table_dtype, num_links, coord_transform, request_hash) + `static read` stub.

## Decisions Made
- `SionnaManager.h` includes `ManifestReader.h`/`SionnaTable.h` (Task 2b files) — Task 2a and 2b were committed back-to-back so the tree is compilable at each commit; 2a's grep-only verification does not require 2b at its commit, but the files were authored together to avoid a non-compiling intermediate state.
- `SionnaManager.cc` includes `<inet/common/InitStages.h>` directly (the project's established include for `inet::INITSTAGE_LOCAL`, per `ChannelAccess.cc`) rather than relying on transitive inclusion.
- Added a small `size()` accessor on `SionnaTable` and a `getTable()` accessor on `SionnaManager` beyond the literal plan spec — minor ergonomics for the 01-03 consumer; no behavior, pure skeleton surface.

## Deviations from Plan

None - plan executed exactly as written. (Verification gates `FEATURE-OK`, `SKELETON-A-OK`, `SKELETON-B-OK` all passed; the only additions are inert accessor methods on the skeleton classes, which carry no behavior.)

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Known Stubs

All four C++ class-pairs are intentional compilable skeletons for this plan; the real logic is filled in by Plan 01-03 (every stub carries a `TODO(01-03)` marker). This is by design per the plan objective, not an incomplete implementation.

| File | Stub | Resolved by |
|------|------|-------------|
| `SionnaChannelModel.cc` | `getAttenuation` delegates to `NrChannelModel::getAttenuation` | 01-03 (`-table_->lookup(...)`) |
| `SionnaManager.cc` | `initialize` INITSTAGE_LOCAL body empty | 01-03 (artifact load + contract assertion) |
| `SionnaTable.cc` | `loadBinary` returns empty, `lookup` returns 0.0 | 01-03 (LE-binary loader + bounds-checked lookup) |
| `ManifestReader.cc` | `read` returns default-constructed `Manifest` | 01-03 (JSON parse + validation) |

## Next Phase Readiness
- SEAM-02 isolation in place; the symbol-check gate is ready to be executed against a real feature-OFF build (Plan 01-04 Task 4) — the genuinely automated SEAM-02 proof.
- The skeleton class surface (inheritance, `@class`/`Define_Module` registration, contract `struct Manifest`, table holder) is ready for Plan 01-03 to fill in the `getAttenuation` override, the artifact loaders, and the fail-loud contract assertions.
- Not yet compiled with the feature ON in this plan (no build run here); Plan 01-04 performs the feature-ON compile + feature-OFF symbol check as the wave-merge gate.

## Self-Check: PASSED

All 12 declared files exist on disk; all 3 task commits (`964091de`, `7b2fbd52`, `d8fa2462`) are present in git history.

---
*Phase: 01-thin-end-to-end-slice-bring-up-empty-world-validation*
*Completed: 2026-06-17*
