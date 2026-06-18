# Phase 2: Full BLER Table & CQI/Feedback Consistency - Context

**Gathered:** 2026-06-18
**Status:** Ready for planning

<domain>
## Phase Boundary

The offline tool produces a BLER value for **every MCS per link** via `sionna.sys.PHYAbstraction`
(effective-SINR → shipped 5G-NR LDPC reference tables, EESM), and **both** the reception path
and the CQI-feedback path read that **one** `SionnaTable`, so the scheduler's chosen MCS and the
realized BLER provably agree.

In scope (Requirements TOOL-04, MOD-03, FB-01):
- Offline: BLER per (link, MCS) for all MCS via `PHYAbstraction`, pinned `mcs_table_index`.
- Reception: `SionnaTable` BLER lookup, retaining the `uniform(0,1) ≤ BLER` success draw and the
  `harqReduction_` HARQ heuristic on top.
- Feedback: `SionnaFeedbackComputation::getCqi()` selects the highest MCS with BLER ≤ `targetBler_`
  from the **same** table; an init self-consistency check confirms agreement.

Out of scope (explicitly deferred): dynamic interference / BLER-vs-SINR curves over a real S axis
(V2-01), MIESM switch (revisit in v2), UL/D2D MCS tables, per-RV HARQ precompute (V2-07), full
LDPC Monte Carlo backend (V2-06). Phase 2 is **DL single-link, noise-limited** — the v1 degenerate
slice of the additive [L,M,S] schema.

</domain>

<decisions>
## Implementation Decisions

### Table keying (CQI vs MCS)
- **D-01:** The `SionnaTable` BLER is keyed **MCS-primary**: `[L, MCS]` (M = MCS index). This is the
  single source of truth, matching `PHYAbstraction`'s native MCS indexing and the scheduler's MCS
  selection. No per-QAM split — each MCS index already carries its (modulation, code rate).
- **D-02:** CQI is **derived**, not stored as a parallel truth: feedback selects an MCS, then maps
  MCS→CQI via Simu5G's existing `NrAmc` CQI↔MCS mapping. Reception looks up BLER by the MCS the
  scheduler actually assigned. Avoids the quantization loss and dual-truth of a CQI-keyed table.

### MCS table selection (mcs_table_index)
- **D-03:** Pin **`mcs_table_index = 2`** (5G-NR 38.214 Table 5.1.3.1-2, **256-QAM**, MCS 0–27
  spanning QPSK→256-QAM). This is a **consistency constraint, not a preference**: Simu5G NR currently
  **hardcodes** `NrMcsTable(extended=true)` (the 256-QAM table) with no NED knob (see code anchors),
  so the Sionna table MUST be generated for the extended MCS-index mapping or the indices won't align
  with the live `NrAmc`. Table 1 (64-QAM) would shift indices vs. the live AMC.
- **D-04:** `mcs_table_index` is carried as a **manifest contract parameter** (SSOT → manifest →
  `SionnaManager` CAL-02 assert), value fixed to 2 while Simu5G hardcodes `extended`. If/when Simu5G
  exposes the `extended` flag as a NED param (the `NrMcs.cc:25` "make it configurable" intent), the
  assert binds to it. The M axis spans the full extended MCS set (0–27) — one table covers all QAM.

### Future-stable lookup interface
- **D-05:** The C++ lookup is **key-based and forward-stable**, so new dimensions grow the *data*, not
  the *interface*: `SionnaTable::lookupBler(query)` where the query carries
  `(linkId, mcsTableIndex, mcsIndex, effectiveSinr)` → maps to schema `[L, M, S]` (+ table-index).
  In v1, `mcsTableIndex` is asserted to the pinned value and `effectiveSinr` falls into the single
  degenerate S bin. In v2, multiple `mcsTableIndex` blocks, real SINR-bin interpolation, and more
  links are all **additive in data** — the call sites and module interface do not change.
- **D-06:** Key by **MCS, not QAM** (modulation is derived from `(mcsTableIndex, mcsIndex)`; keying by
  QAM would lose the code-rate resolution). The manifest declares the **active dimensions**
  (`num_mcs`, `num_sinr_bins`, `mcs_table_index`, link list) so `SionnaTable` can **fail-loud
  (`cRuntimeError`) on an out-of-range / invalid query key** and stay self-describing.

### Effective-SINR mapping (EESM vs MIESM)
- **D-07:** Use **EESM** (the `PHYAbstraction` default), **recorded as a manifest contract parameter**
  (`sinr_effective_fun`) for reproducibility/provenance. This is an **offline-tool decision** — Simu5G
  reads precomputed BLER and never re-derives the mapping. In the v1 empty world (`fading=false`,
  noise-limited) the per-subcarrier SINR is flat, so EESM ≈ MIESM; the distinction only matters under
  frequency-selective SINR (v2). Revisit MIESM in v2 if 256-QAM calibration shows bias.

### Self-consistency check (FB-01)
- **D-08:** On any link where the feedback-selected MCS has table BLER > `targetBler_`, the init
  self-check **aborts with `cRuntimeError`** (fail-loud, CAL-02 style). By construction the check is
  near-tautological when both readers share the same table + `targetBler_`; a violation therefore
  signals a real wiring/logic bug (the two readers disagree) and must not pass silently.
- **D-09:** The operating point is the **inherited `targetBler_`** (`LteRealisticChannelModel.ned`
  default 0.01, ini-overridable) — a **single source** shared by `getCqi()`, the self-check, and the
  rest of the stack (no separate Sionna-specific target). Comparison is **exact** (`BLER ≤ targetBler_`
  on the identical stored `double` — no float drift, no epsilon).

### Claude's Discretion
- Exact C++ class/method layout of `SionnaFeedbackComputation` and how it hooks Simu5G's feedback
  generation (`LteFeedbackComputationRealistic` / `LteDlFeedbackGenerator`) — planner/researcher.
- HDF5/manifest schema field names for the M and S axes and the `mcs_table_index` block — planner,
  consistent with the existing `[L,M,S]` reservation and ART-01/ART-02.
- Whether the SINR→effective-SINR happens entirely inside `PHYAbstraction` (expected) vs. any glue.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 2: Full BLER Table & CQI/Feedback Consistency" — goal + 4 success criteria.
- `.planning/REQUIREMENTS.md` — TOOL-04, MOD-03, FB-01 (Phase 2); MOD-02/TOOL-05/REP-01/REP-02
  (Phase 3 neighbors); V2-01/V2-06/V2-07 (deferred extensions this design stays additive toward).

### Project contract & stack decisions
- `CLAUDE.md` — locked: `sionna.sys.PHYAbstraction` (EESM + shipped 5G-NR LDPC tables, **not**
  hand-rolled, **not** LDPC Monte Carlo = V2-06); post-equalization SINR x-axis contract; the
  `/bler_curve[L,M,S]` over `/sinr_grid[S]` additive schema with degenerate v1 S; MIESM as the
  `sinr_effective_fun` swap option; "pin mcs_table_index once, identically, on both sides"; fail-loud.

### Phase 1 artifacts this builds on
- `.planning/phases/01-thin-end-to-end-slice-bring-up-empty-world-validation/01-SUMMARY.md` files
  (01-01 … 01-04) — the offline tool, artifact/manifest contract, `SionnaTable`/`SionnaManager`/
  `SionnaChannelModel`, and CAL-02 assertion this phase extends.
- `tools/sionna_precompute/precompute.py`, `scenario.example.json` — the SSOT + producer to extend
  with the per-MCS BLER computation (TOOL-04).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/simu5g/stack/mac/amc/NrMcs.{h,cc}` — `NrMcsTable(bool extended=true)`: one table spans QPSK→
  256-QAM; `cqiTable[0..15]`, `table[]` (MCS→mod/rate), `getMinIndex/getMaxIndex(mod)`. Source of the
  CQI↔MCS mapping for D-02 and the pinned-table reality for D-03 (`NrMcs.cc:24-65`, `:124-175`).
- `src/simu5g/stack/mac/amc/NrAmc.{h,cc}` — holds `dlNrMcsTable_/ulNrMcsTable_/d2dNrMcsTable_`
  (default-constructed `extended=true`, no NED param — `NrAmc.h:38-40`); CQI→MCS selection logic at
  `NrAmc.cc:243-266`. The CQI-derivation (D-02) reuses this.
- `src/simu5g/stack/phy/feedback/LteFeedbackComputationRealistic.{h,cc}`, `LteDlFeedbackGenerator.*`
  — the feedback-computation seam that `SionnaFeedbackComputation::getCqi()` (FB-01) plugs into.
- `src/simu5g/stack/phy/channelmodel/sionna/SionnaTable.*`, `SionnaManager.*`, `SionnaChannelModel.*`
  — Phase-1 consumer half: extend `SionnaTable` with the `[L,M]` BLER + key-based `lookupBler` (D-05);
  `SionnaManager` already does the CAL-02 manifest assert to extend for `mcs_table_index`/dims (D-06).

### Established Patterns
- Reception BLER is currently **CQI-keyed**: `binder_->phyPisaData.getBler(itxmode, cqi, snr)` at
  `LteRealisticChannelModel.cc:1796,1947`; `effectiveErrorRateWithHarq = per * pow(harqReduction_,
  attempt-1)` at `:1817,:1969`. MOD-03 swaps the *source* (SionnaTable, MCS-keyed) while keeping the
  `uniform(0,1) ≤ BLER` draw + `harqReduction_` on top.
- Operating point: `targetBler default(0.01)`, `harqReduction default(0.2)` at
  `LteRealisticChannelModel.ned:51,53` — inherited by `SionnaChannelModel` (D-09).
- Fail-loud contract assertion pattern already established in Phase 1 (`SionnaManager`
  `assertContractMatchesLiveScenario`, CAL-02) — extend it for the new contract dimensions.

### Integration Points
- Offline `precompute.py` gains a per-MCS BLER stage via `sionna.sys.PHYAbstraction` (TOOL-04),
  writing `[L,M]` (degenerate S) into the existing HDF5 + LE-binary + manifest artifact.
- `SionnaChannelModel` reception path calls `SionnaTable::lookupBler(query)` instead of `phyPisaData`.
- New `SionnaFeedbackComputation` reads the **same** `SionnaTable` for `getCqi()`; `SionnaManager`
  runs the init self-consistency check across all links.

</code_context>

<specifics>
## Specific Ideas

- The lookup-key design (D-05/D-06) is explicitly motivated by "the query should carry the
  discriminating info (MCS, table-index, SINR) so the Sionna-providing module never needs extending
  later — only the data grows." This is the guiding principle for the C++ interface.
- The empty-world ~70 dB DL SINR (from Phase 1) means essentially all MCS have BLER≈0; the 256-QAM
  table is chosen partly so the top MCS is actually reachable/exercised, not just QPSK.

</specifics>

<deferred>
## Deferred Ideas

- **Multiple `mcs_table_index` in one artifact** (Table 1 AND Table 2 selectable) — additive schema
  reservation, but YAGNI now since Simu5G hardcodes `extended=true`. Revisit if/when the `extended`
  flag is exposed as a NED param.
- **Expose Simu5G's `NrMcsTable` `extended` flag as a NED parameter** — small upstream improvement
  that would make `mcs_table_index` a genuine runtime choice (and the CAL-02 assert bind to it).
  Currently hardcoded; not required for Phase 2.
- **MIESM** (`sinr_effective_fun=MIESM`) — meaningful only under frequency-selective SINR (v2); the
  manifest parameter (D-07) makes the switch a data/regenerate change, not a code change.
- **UL / D2D MCS tables** (`ulNrMcsTable_`, `d2dNrMcsTable_`) — Phase 2 is DL-only; per-direction
  contract entries deferred.
- **Real S (SINR-bin) axis / dynamic interference** (V2-01) — the degenerate S and the key-based
  lookup are designed so this is purely additive.

None of these are scope creep into Phase 2 — they are forward-compatibility notes.

</deferred>

---

*Phase: 2-full-bler-table-cqi-feedback-consistency*
*Context gathered: 2026-06-18*
