# Walking Skeleton — Simu5G × Sionna RT Integration

**Phase:** 1
**Generated:** 2026-06-17

## Capability Proven End-to-End

A researcher authors one shared scenario, runs the offline Sionna tool over an empty world with a single Tx/Rx link, produces a versioned artifact (HDF5 + JSON manifest + LE-binary table), selects `SionnaChannelModel` via an ini string, and runs a single-link NR simulation whose per-link path gain comes from Sionna — trusting it because the empty-world Friis round-trip and the fail-loud manifest assertion both pass, while a default build/run stays provably free of Python/HDF5.

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Channel-model seam | Existing NED parametric typename `<nrChannelModelType> like ILteChannelModel`; new `simple SionnaChannelModel extends NrChannelModel @class("SionnaChannelModel")` | SEAM-01 needs no NED interface edit; the ini string selects the class. (RESEARCH, verified in code.) |
| Override point | Override `getAttenuation()` only; reuse inherited `getSINR`/`getRSRP` | MOD-01 — Sionna owns path gain, Simu5G owns SINR. `getAttenuation` is the single funnel all of getSINR/getRSRP feed through. |
| Build isolation | `.oppfeatures` feature `Simu5G_Sionna` (`initiallyEnabled="false"`, `extraSourceFolders` = the sionna source folder, `compileFlags="-DWITH_SIONNA"`), mirroring `Simu5G_Cars` | SEAM-02 — default deep build links zero Sionna/HDF5/Python symbols. |
| Artifact format (C++ reader) | Binary + JSON: little-endian float64 `path_gain.bin` table + `manifest.json` sidecar; HDF5 (`results.h5`) kept as canonical/debug on the Python side only | HDF5/HighFive absent system-wide → adding it would threaten SEAM-02. Binary+JSON adds zero system deps. (Assumption A2.) |
| Manifest contract | `schema_version`, `coord_transform`, full parameter contract, `request_hash`, degenerate `sinr_grid` (S=1) | ART-02 — v2 interference curves become a purely additive `[L,S]` extension; foundational and shipped in minimal form now. |
| Contract assertion owner | Thin `SionnaManager` cSimpleModule asserts at `INITSTAGE_LOCAL`, `cRuntimeError` on any mismatch, no silent fallback | CAL-02; forward-compatible with N-link tables in Phase 2/3 (Assumption A6). |
| Coordinate transform | Explicit `coord_transform` (identity/offset for empty world) recorded in the manifest; distance computed via `phy_->getCoord().distance(coord)` | TOOL-02; the ~0.04 dB Friis residual is the empirical transform-correctness proof. |
| Statistical-term suppression (Phase 1) | ini-level: `shadowing=false`, `fading=false`, `dynamicLos=false`, `fixedLos=true` | A3 — full source-level suppression (MOD-02) is deferred to Phase 3 per the roadmap; ini-level keeps the Friis round-trip clean now. |
| Subcarrier representation | Single representative subcarrier at band center, pinned identically in the manifest and CAL-01 | A4 — v1 collapses the per-RB grid to one figure. |
| Offline tool location | `tools/sionna_precompute/` (outside the Simu5G build), run against the pinned venv at `/home/zoli/Projects/OMNET/Sionna/venv` | Hard constraint: no Python/TF/GPU in the build or runtime; coupling is the cached artifact only. |
| Directory layout | C++: `src/simu5g/stack/phy/channelmodel/sionna/{SionnaChannelModel,SionnaManager,SionnaTable,ManifestReader}.*`; tool: `tools/sionna_precompute/`; sim: `simulations/NR/sionna/` | Mirrors existing channelmodel package; feature folder cleanly excludable. |

## Stack Touched in Phase 1

- [x] Project scaffold — `.oppfeatures` feature, sionna source folder, offline tool dir, sim config dir (Plans 01-01, 01-02)
- [x] One real "DB" write — the offline tool writes the artifact (HDF5 + manifest.json + path_gain.bin) (Plan 01-01)
- [x] One real "DB" read — `ManifestReader`/`SionnaTable` load + validate the artifact at init (Plan 01-03)
- [x] One real interaction wired through the seam — ini selects `SionnaChannelModel`; `getAttenuation` returns the table path gain consumed by inherited `getSINR` (Plans 01-03, 01-04)
- [x] Full-stack run — single-link NR simulation runs end-to-end on the Sionna path gain; default-build symbol check + Friis round-trip + fingerprint baseline (Plans 01-02, 01-04)

## Out of Scope (Deferred to Later Slices)

- Per-(link, MCS) BLER table via `sionna.sys.PHYAbstraction` and CQI/feedback consistency → Phase 2
- Source-level suppression of statistical path-loss terms (MOD-02), two-seed determinism (REP-02), request-hash cache (TOOL-05), pinned Sionna fingerprint baselines (REP-01) → Phase 3
- Authored real-map (Munich) scene, path-gain RSRP, opt-in auto-invocation, calibration report → Phase 4
- Interference curves, mobility/Doppler, per-RB MIMO, INET-scene derivation → v2 (out of roadmap)
- HDF5 C++ reader / HighFive in the Simu5G build (only revisited if HDF5 is deliberately adopted as a build dep)
- Multi-link / N-Tx-N-Rx batching (v1 is a single link; the table schema is `[L]`, v2-ready)

## Subsequent Slice Plan

Each later phase adds one vertical slice on top of this skeleton without altering its architectural decisions:

- Phase 2: The offline tool computes BLER for every CQI/MCS per link via `PHYAbstraction`; both reception and CQI feedback read the same `SionnaTable` (extends the table from `[L]` path gain to per-(link,MCS) BLER).
- Phase 3: Suppress statistical terms at the source (MOD-02), prove two-seed bit-identical RSRP (REP-02), add the request-hash cache (TOOL-05), pin Sionna fingerprint baselines (REP-01) — all on the unchanged seam.
- Phase 4: Authored Munich scene (DIF-01), path-gain RSRP (DIF-02), opt-in auto-invocation (DIF-03), bounded-difference calibration report (DIF-04) — strictly additive differentiators.
