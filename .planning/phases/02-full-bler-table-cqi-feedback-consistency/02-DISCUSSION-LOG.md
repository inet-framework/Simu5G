# Phase 2 Discussion Log

**Date:** 2026-06-18
**Mode:** discuss (default, interactive)

Human-reference audit of the discussion. Not consumed by downstream agents — see `02-CONTEXT.md`
for the decisions that drive research and planning.

## Areas selected

All four offered gray areas were selected: Table keying (CQI vs MCS), MCS table (mcs_table_index),
EESM vs MIESM, Self-consistency check behavior.

## Area 1 — Table keying (CQI vs MCS)

- Options: MCS-primary (CQI derived) / CQI-primary (15) / store both axes.
- **Selected:** MCS-primary, CQI derived via `NrAmc` CQI↔MCS. → D-01, D-02.
- Note: surfaced that current reception is CQI-keyed (`phyPisaData.getBler(txmode,cqi,snr)`), but the
  scheduler assigns MCS and `PHYAbstraction` is MCS-indexed → MCS is the natural single truth.

## Area 2 — MCS table (mcs_table_index) + the multi-QAM question

- Initial options: Table 1 (64-QAM) / Table 2 (256-QAM) / Table 3 (low-SE).
- User asked: can it be a parameter? → Yes, established as a manifest contract parameter (D-04).
- User asked: can Simu5G use multiple QAM in parallel; should we generate multiple QAM tables?
  Investigated `simulations`/`showcases`/`tutorials` (40 inis) + `NrMcs`/`NrAmc`:
  - No config selects an MCS table / 256-QAM / per-direction table; only `amcMode`, fixed CQI.
  - `NrMcsTable(extended=true)` is **hardcoded** (no NED param); one table spans QPSK→256-QAM and the
    AMC picks per link. So "multiple QAM" already happens within one table — no separate per-QAM
    tables needed.
- **Selected:** `mcs_table_index = 2` (extended/256-QAM), value pinned to match Simu5G's hardcoded
  extended table, M = full extended MCS set. → D-03, D-04.

## Area 2b — Future-stable lookup interface

- User asked: can the Simu5G→table lookup carry the discriminating info (QAM/MCS) so the Sionna
  module never needs extending later?
- **Agreed:** key-based `SionnaTable::lookupBler((link, mcsTableIndex, mcs, sinr))`, degenerate
  table-index/S in v1, additive in data not code; key by MCS (QAM derived); manifest declares active
  dims for fail-loud query validation. → D-05, D-06.

## Area 3 — EESM vs MIESM

- Options: EESM (recorded as manifest param) / MIESM now / EESM hardcoded.
- **Selected:** EESM, recorded as `sinr_effective_fun` manifest parameter. → D-07.
- Note: v1 flat-SINR empty world makes EESM ≈ MIESM; it is an offline-only decision (Simu5G reads
  precomputed BLER). Revisit MIESM in v2.

## Area 4 — Self-consistency check behavior

- Q1 (on violation): hard `cRuntimeError` abort / warn+continue. **Selected:** hard abort. → D-08.
- Q2 (operating point + tolerance): user asked for an explanation; after it, **selected** inherited
  `targetBler_` (0.01) single source + exact comparison (no epsilon). → D-09.

## Deferred ideas

Multiple `mcs_table_index` per artifact; exposing the `extended` NED flag; MIESM (v2); UL/D2D tables;
real S axis / dynamic interference (V2-01). See `02-CONTEXT.md` <deferred>.

## Scope creep

None — discussion stayed within the Phase 2 boundary.
