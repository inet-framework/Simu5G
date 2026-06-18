# Phase 2: Full BLER Table & CQI/Feedback Consistency - Research

**Researched:** 2026-06-18
**Domain:** Sionna PHYAbstraction (EESM → 5G-NR LDPC BLER) offline; Simu5G C++ reception + DL feedback consistency
**Confidence:** HIGH (the load-bearing PHYAbstraction API and the Simu5G reception/feedback seams were verified against the installed venv and the actual source tree this session)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** `SionnaTable` BLER is keyed **MCS-primary** `[L, MCS]` (M = MCS index). Single source of truth, matching `PHYAbstraction`'s native MCS indexing and the scheduler's MCS selection. No per-QAM split — each MCS index carries its (modulation, code rate).
- **D-02:** CQI is **derived**, not stored as a parallel truth: feedback selects an MCS, then maps MCS→CQI via Simu5G's existing `NrAmc` CQI↔MCS mapping. Reception looks up BLER by the MCS the scheduler actually assigned. Avoids quantization loss and dual-truth.
- **D-03:** Pin **`mcs_table_index = 2`** (38.214 Table 5.1.3.1-2, **256-QAM**, MCS 0–27 QPSK→256-QAM). A **consistency constraint, not a preference**: Simu5G NR hardcodes `NrMcsTable(extended=true)` (the 256-QAM table) with no NED knob, so the Sionna table MUST be generated for the extended MCS-index mapping.
- **D-04:** `mcs_table_index` carried as a **manifest contract parameter** (SSOT → manifest → `SionnaManager` CAL-02 assert), value fixed to 2 while Simu5G hardcodes `extended`. The M axis spans the full extended MCS set (0–27).
- **D-05:** C++ lookup is **key-based and forward-stable**: `SionnaTable::lookupBler(query)` where the query carries `(linkId, mcsTableIndex, mcsIndex, effectiveSinr)` → schema `[L, M, S]` (+ table-index). v1 asserts `mcsTableIndex` to the pinned value; `effectiveSinr` falls in the single degenerate S bin. v2 grows the *data*, not the interface.
- **D-06:** Key by **MCS, not QAM**. Manifest declares **active dimensions** (`num_mcs`, `num_sinr_bins`, `mcs_table_index`, link list) so `SionnaTable` can **fail-loud (`cRuntimeError`) on an out-of-range / invalid query key**.
- **D-07:** Use **EESM** (`PHYAbstraction` default), **recorded as a manifest contract parameter** (`sinr_effective_fun`). Offline-tool decision; Simu5G reads precomputed BLER, never re-derives. In v1 empty world EESM ≈ MIESM (flat SINR).
- **D-08:** On any link where feedback-selected MCS has table BLER > `targetBler_`, the init self-check **aborts with `cRuntimeError`** (fail-loud, CAL-02 style). Near-tautological by construction; a violation signals a real wiring/logic bug.
- **D-09:** Operating point is the **inherited `targetBler_`** (`LteRealisticChannelModel.ned` default 0.01, ini-overridable) — a **single source** shared by `getCqi()`, the self-check, and the rest of the stack. Comparison is **exact** (`BLER ≤ targetBler_` on the identical stored `double`).

### Claude's Discretion
- Exact C++ class/method layout of `SionnaFeedbackComputation` and how it hooks `LteFeedbackComputationRealistic` / `LteDlFeedbackGenerator`.
- HDF5/manifest schema field names for the M and S axes and the `mcs_table_index` block (consistent with `[L,M,S]` and ART-01/ART-02).
- Whether SINR→effective-SINR happens entirely inside `PHYAbstraction` (expected) vs. any glue.

### Deferred Ideas (OUT OF SCOPE)
- Multiple `mcs_table_index` in one artifact (Table 1 AND Table 2 selectable) — additive reservation, YAGNI now.
- Expose Simu5G's `NrMcsTable` `extended` flag as a NED parameter.
- **MIESM** (`sinr_effective_fun=MIESM`) — meaningful only under frequency-selective SINR (v2).
- UL / D2D MCS tables (`ulNrMcsTable_`, `d2dNrMcsTable_`) — Phase 2 is DL-only.
- Real S (SINR-bin) axis / dynamic interference (V2-01) — additive by design.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| **TOOL-04** | Tool computes BLER per (link, MCS) for all MCS via `sionna.sys.PHYAbstraction` (effective-SINR → shipped 5G-NR LDPC tables, EESM) | Verified `PHYAbstraction.__init__`/`__call__` signatures and a working DL call in the venv (§Code Examples). Produces `bler[L,M]` over MCS 2–27 for DL table 2. Stages onto the existing `precompute.py` artifact writer (§Architecture: Offline stage). |
| **MOD-03** | BLER from a `SionnaTable` lookup, keeping the `uniform(0,1) ≤ BLER` draw + `harqReduction_` | Verified the reception seam is `LteRealisticChannelModel::isReceptionSuccessful` at lines 1709/1853; the swappable call is `binder_->phyPisaData.getBler(itxmode, cqi, snr)` at :1796/:1947. The draw + harq math (`:1817`, `:1969`, `:1838`) is downstream and reusable verbatim (§Architecture: Reception seam). |
| **FB-01** | `SionnaFeedbackComputation::getCqi()` selects highest MCS with BLER ≤ `targetBler_` from the **same** `SionnaTable`; init self-consistency check confirms agreement | Verified the feedback seam is `LteFeedbackComputationRealistic::getCqi(txmode, snr)` (`:83`), instantiated by a **hardcoded C++ factory** (`LtePhyEnb::initializeFeedbackComputation` `:348`, not a NED typename). FB-01 requires a factory/instantiation change, not an ini-only swap (§Pitfall 1, §Open Question 2). |
</phase_requirements>

## Summary

Phase 2 turns the Phase-1 single-scalar path-gain table into a full per-MCS BLER table and wires it into **two** Simu5G readers — the reception error model and the DL CQI-feedback generator — so the scheduler's chosen MCS and the realized BLER provably agree. The offline half is well-supported: `sionna.sys.PHYAbstraction` (verified 2.0.1 in the venv) takes an effective SINR + MCS index + `(mcs_category, mcs_table_index)` and returns BLER directly from calibrated 5G-NR LDPC reference tables, deterministically. The C++ half is a clean extension of the established Phase-1 pattern (`SionnaTable` + `SionnaManager` + fail-loud contract assertion).

The single biggest design realization from grounding the code: **DL is `mcs_category=1` (PDSCH) in Sionna's `MCSDecoderNR`, not 0** — the opposite of the file-naming intuition (`category=0`/PUSCH). With DL=`(category=1, table_index=2)` the shipped PDSCH table covers **MCS 2–27** with monotone, deterministic BLER; **MCS 0 and 1 are absent and return the sentinel `inf`**. Independently, in Simu5G's data model the **reception path and the on-the-wire `UserTxParams` only carry a CQI (1–15), never an explicit MCS index 0–27** — so D-01/D-02's "look up BLER by the MCS the scheduler assigned" requires an explicit CQI→MCS-index bridge in C++ (Simu5G's `NrMcsElem` stores `(mod, coderate)`, not an index). These two facts are the load-bearing planning inputs.

**Primary recommendation:** Generate the DL BLER table with `PHYAbstraction(...)` using `mcs_category=1, mcs_table_index=2`, MCS 2–27, at the single empty-world effective SINR; write `bler[L,M]` (degenerate S) into the existing artifact alongside `path_gain.bin`; add `mcs_table_index`, `mcs_category`, `sinr_effective_fun`, `num_mcs` to the manifest contract. On the C++ side, add `SionnaTable::lookupBler(query)`; swap the `getBler(...)` call in `isReceptionSuccessful` for it (keeping the draw + harq); subclass `LteFeedbackComputationRealistic` as `SionnaFeedbackComputation` overriding `getCqi`; instantiate it via a (small) change to the hardcoded feedback factory in `LtePhyEnb`; and run the D-08 self-consistency assert in `SionnaManager` at init. Resolve the CQI↔MCS-index bridge explicitly (Open Question 1) before writing tasks.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Compute BLER per (link, MCS) | Offline Python tool (`precompute.py`) | — | Sionna/PyTorch must stay outside the Simu5G build (SEAM-02); precompute-once. |
| Persist BLER `[L,M]` + contract | Offline tool → artifact (HDF5 + LE-binary + manifest) | — | Self-describing, fail-loud-on-mismatch exchange format (ART-01/ART-02). |
| Load + contract-assert the table | `SionnaManager` (C++, init) | — | Single artifact-load + CAL-02 assertion point (established Phase 1). |
| Hold + key-based BLER lookup | `SionnaTable` (C++ utility) | — | Plain in-memory holder, no kernel/Sionna headers; bounds-checked. |
| Reception BLER → success draw | `SionnaChannelModel` / inherited `LteRealisticChannelModel::isReceptionSuccessful` | `SionnaTable` | Reception math (draw, harq, per-RB success) stays in the channel model; only the BLER *source* is swapped (MOD-03). |
| DL CQI feedback (highest MCS ≤ targetBler) | `SionnaFeedbackComputation` (subclass of `LteFeedbackComputationRealistic`) | `SionnaTable` | CQI is computed gNB-side by the feedback-computation object; same table, second reader (FB-01). |
| MCS↔CQI mapping | `NrAmc` / `NrMcsTable` (existing) | — | Simu5G owns the 5G-NR CQI/MCS tables; reused, not duplicated (D-02). |
| Init self-consistency check | `SionnaManager` (C++, init) | `NrAmc`, `SionnaTable` | One place that can see both the table and the AMC mapping at init; fail-loud (D-08). |

## Standard Stack

### Core (offline tool — already installed/pinned in Phase 1; NO new packages)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `sionna` (phy+sys) | 2.0.1 | `sionna.sys.PHYAbstraction` (EESM + shipped 5G-NR LDPC BLER tables) | The entire effective-SINR→BLER step as a supported, calibrated library. `[VERIFIED: venv introspection]` |
| `sionna-rt` | 2.0.1 | RT path solving (Phase 1, unchanged here) | Already used for path gain. `[VERIFIED: venv]` |
| `torch` | 2.12.0 | Tensor backend for `sionna.phy`/`sionna.sys` | PHYAbstraction inputs/outputs are `torch` tensors. `[VERIFIED: venv]` |
| `numpy` | 2.4.6 | Extract BLER to plain arrays for the binary table | `np.asarray(out).reshape(-1)`. `[VERIFIED: venv]` |
| `h5py` | 3.16.0 | Canonical HDF5 artifact writer | Established Phase 1. `[VERIFIED: venv]` |

### Supporting (C++ consumer — all existing in-tree)
| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `nlohmann/json` 3.9.1 (in-tree, MEC) | Manifest parse | Reused from Phase 1; zero default-build symbols. `[VERIFIED: codebase]` |
| `NrAmc` / `NrMcsTable` (`src/simu5g/stack/mac/amc/`) | CQI↔MCS(mod,coderate) mapping | D-02 derivation + self-check. `[VERIFIED: codebase]` |
| `LteFeedbackComputationRealistic` | Base for `SionnaFeedbackComputation` | FB-01. `[VERIFIED: codebase]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| EESM (PHYAbstraction default) | MIESM (`sinr_effective_fun=MIESM(...)`) | Deferred to v2 (D-07); irrelevant under flat empty-world SINR. |
| `sinr_eff` + `num_allocated_re` call path | per-subcarrier `sinr` tensor | In v1 empty world the SINR is flat, so the effective-SINR path is exact and simpler; the per-subcarrier path is the v2 hook (frequency-selective). `[VERIFIED: venv docstring]` |

**Installation:** None. All offline packages are already pinned in `tools/sionna_precompute/requirements.txt`. The C++ side adds no new dependency.

**Version verification:** `sionna.__version__ == 2.0.1` confirmed live in `/home/zoli/Projects/OMNET/Sionna/venv` this session. `[VERIFIED: venv introspection]`

## Package Legitimacy Audit

> Phase 2 installs **no new external packages**. The offline tool reuses the exact Phase-1-pinned venv (`sionna==2.0.1`, `sionna-rt==2.0.1`, `torch==2.12.0`, `numpy==2.4.6`, `h5py==3.16.0`); the C++ side adds none.

| Package | Registry | Disposition |
|---------|----------|-------------|
| (none added) | — | N/A — no new installs in this phase |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
 OFFLINE (Sionna venv — outside the Simu5G build)
 ─────────────────────────────────────────────────
 scenario.json (SSOT)
   │  (carrier, SCS, positions, antenna, mcs_set,
   │   + NEW: mcs_table_index=2, mcs_category=1/DL, sinr_effective_fun)
   ▼
 precompute.py
   ├─► PathSolver (Phase 1)  ──► per-link path gain dB  ──┐
   │                                                      │
   └─► NEW per-MCS BLER stage:                            │
         effective SINR (empty-world, noise-limited)      │
              │                                           │
              ▼                                           │
         PHYAbstraction(category=1, table_index=2)        │
              │  for each MCS in 2..27                    │
              ▼                                           │
         bler[L, M]  (M = MCS index; degenerate S=1)      │
                                                          ▼
                              ┌──────────  ARTIFACT  ───────────┐
                              │ results.h5 (canonical)          │
                              │ path_gain.bin  (LE f64 [L])     │
                              │ bler.bin       (LE f64 [L,M])   │  ← NEW
                              │ manifest.json  (+ mcs_table_index,
                              │   mcs_category, num_mcs,
                              │   sinr_effective_fun, ...)      │  ← extended
                              └─────────────────────────────────┘
 ════════════════════════════════════════════════════════════════
 SIMU5G C++ (Simu5G_Sionna feature ON)
 ─────────────────────────────────────────────────
   SionnaManager::initialize  (INITSTAGE_LOCAL)
     ├─ ManifestReader::read + assertContractMatchesLiveScenario  (extend: mcs_table_index, num_mcs)
     ├─ SionnaTable::loadBinary (path_gain.bin) + loadBlerBinary (bler.bin)   ← NEW
     └─ NEW D-08 self-consistency check (across links): for each link,
        cqi = SionnaFeedbackComputation::getCqi at empty-world SINR
        mcs = NrAmc CQI→MCS map → assert table.lookupBler(link,mcs) ≤ targetBler_
                              │ (one table, validated once)
        ┌─────────────────────┴─────────────────────┐
        ▼                                            ▼
   RECEPTION  (MOD-03)                          FEEDBACK  (FB-01)
   SionnaChannelModel  (inherits               SionnaFeedbackComputation
   LteRealisticChannelModel::                  : LteFeedbackComputationRealistic
   isReceptionSuccessful)                        getCqi(txmode, snr) override:
     cqi (from UserTxParams) ──► MCS              for mcs in 2..27:
     bler = table.lookupBler(link,mcs,sinr)         if table.lookupBler(...,mcs) ≤ targetBler_
     per  = 1-(1-bler)^alloc                          best = mcs
     perHarq = per * harqReduction^(tx-1)           return MCS→CQI (NrAmc)
     receptionFailed = uniform(0,1) ≤ perHarq     instantiated by feedback factory
                                                  (LtePhyEnb::initializeFeedbackComputation)
```

### Component Responsibilities
| File / Function (anchor) | Responsibility | Phase-2 change |
|--------------------------|----------------|----------------|
| `tools/sionna_precompute/precompute.py` | Offline producer | ADD per-MCS BLER stage (TOOL-04); write `bler.bin` `[L,M]`; extend manifest |
| `SionnaTable.{h,cc}` | In-memory table + lookup | ADD `bler_` `[L*M]`, `loadBlerBinary`, `lookupBler(query)` (D-05/D-06) |
| `ManifestReader.{h,cc}` | Manifest struct + parse | ADD `mcs_table_index`, `mcs_category`, `num_mcs`, `sinr_effective_fun`, `bler_table_path` |
| `SionnaManager.{h,cc}` `:98,:137` | Load + CAL-02 assert | EXTEND `assertContractMatchesLiveScenario`; ADD D-08 self-check |
| `LteRealisticChannelModel::isReceptionSuccessful` `:1709` (`_D2D` `:1853`) | Reception error model | SWAP `getBler(itxmode,cqi,snr)` `:1796` for Sionna lookup (MOD-03), keep draw + harq |
| `LteFeedbackComputationRealistic::getCqi` `:83` | gNB-side CQI from BLER | SUBCLASS → `SionnaFeedbackComputation::getCqi` (FB-01) |
| `LtePhyEnb::initializeFeedbackComputation` `:340-353` | Feedback-computation factory | CHANGE to instantiate the Sionna feedback computation when active (Open Q 2) |
| `NrAmc::getMcsElemPerCqi` `:241` / `NrMcsTable` (`NrMcs.cc:24`) | CQI↔(mod,coderate) | REUSE for D-02 CQI↔MCS-index bridge |

### Recommended Project Structure
```
tools/sionna_precompute/
├── precompute.py                 # + compute_bler_table(scenario, eff_sinr) stage
├── tests/test_bler_table.py      # NEW: pytest (requires_venv) — monotonicity, determinism, inf-handling
src/simu5g/stack/phy/channelmodel/sionna/
├── SionnaTable.{h,cc}            # + bler_, loadBlerBinary, lookupBler(query)
├── ManifestReader.{h,cc}         # + mcs_table_index/category/num_mcs/sinr_effective_fun
├── SionnaManager.{h,cc}          # + extended contract assert + D-08 self-check
├── SionnaFeedbackComputation.{h,cc}  # NEW: getCqi override (FB-01)
tests/sionna/unit/
├── test_bler_lookup.cc           # NEW: lookupBler bounds/keys, MCS→CQI selection
```

### Pattern 1: PHYAbstraction effective-SINR → BLER per MCS (TOOL-04)
**What:** For a single (link) effective SINR, sweep MCS 2–27 and read `bler` out.
**When to use:** The offline per-MCS stage; once per link in the empty-world v1.
**Example:**
```python
# Source: VERIFIED against installed sionna 2.0.1 (PHYAbstraction.__call__ docstring + live run)
import numpy as np, torch
from sionna.sys import PHYAbstraction

phy = PHYAbstraction()  # default EESM + MCSDecoderNR + shipped tables (load_bler_tables_from='default')

def bler_dl(mcs_index, eff_sinr_linear, table_index=2, category=1):  # DL = PDSCH = category 1
    sinr_eff = torch.tensor([eff_sinr_linear], dtype=torch.float32)
    num_re   = torch.tensor([16800], dtype=torch.int32)   # allocated REs in the slot
    mcs      = torch.tensor([mcs_index], dtype=torch.int32)
    out = phy(mcs_index=mcs, sinr_eff=sinr_eff, num_allocated_re=num_re,
              mcs_table_index=table_index, mcs_category=category,
              check_mcs_index_validity=False)
    # out = (num_decoded_bits, harq_feedback, sinr_eff, tbler, bler)
    return float(np.asarray(out[4]).reshape(-1)[0])   # out[4] = bler
```
**Notes (VERIFIED this session):**
- Output tuple order: `(num_decoded_bits, harq_feedback, sinr_eff, tbler, bler)`. Use `bler` (`out[4]`); `tbler` (`out[3]`) is the transport-BLER (≥ per-codeblock BLER; equal when 1 code block).
- **`mcs_category=1` is PDSCH/DL, `0` is PUSCH/UL** (per `MCSDecoderNR` docstring — *inverse of file naming*). DL table 2 covers **MCS 2–27**.
- Output is a 1-element tensor; extract with `np.asarray(...).reshape(-1)[0]` (a bare `float(np.asarray(...))` on a 1-D array raises `TypeError`).

### Pattern 2: Key-based forward-stable lookup (D-05/D-06)
**What:** `SionnaTable::lookupBler(query)` where `query = {linkId, mcsTableIndex, mcsIndex, effectiveSinr}`.
**When to use:** Both readers (reception + feedback) call this; never index the raw vector.
**Example (sketch):**
```cpp
// Source: extends the established Phase-1 SionnaTable::lookup bounds-check pattern (SionnaTable.cc:67)
struct BlerQuery { std::size_t linkId; int mcsTableIndex; int mcsIndex; double effectiveSinr; };

double SionnaTable::lookupBler(const BlerQuery& q) const {
    if (q.mcsTableIndex != pinnedMcsTableIndex_)      // v1: assert pinned (D-05)
        throw cRuntimeError("SionnaTable: mcsTableIndex %d != pinned %d", q.mcsTableIndex, pinnedMcsTableIndex_);
    if (q.linkId >= numLinks_ || q.mcsIndex < mcsMin_ || q.mcsIndex > mcsMax_)
        throw cRuntimeError("SionnaTable: out-of-range BLER query (link %zu, mcs %d)", q.linkId, q.mcsIndex);
    // v1: single degenerate S bin; effectiveSinr currently unused except for future interpolation
    return bler_[q.linkId * numMcs_ + (q.mcsIndex - mcsMin_)];
}
```

### Pattern 3: `getCqi` override reading the same table (FB-01)
**What:** Highest MCS with table BLER ≤ `targetBler_`, then MCS→CQI.
**Anchor:** mirrors the base `LteFeedbackComputationRealistic::getCqi` (`:83`) which already does an argmin over `phyPisaData_->getBler(...)` vs `targetBler_`.
**Example (sketch):**
```cpp
// Source: subclass of LteFeedbackComputationRealistic (LteFeedbackComputationRealistic.cc:83)
Cqi SionnaFeedbackComputation::getCqi(TxMode txmode, double snr) {
    int best = -1;
    for (int mcs = mcsMin_; mcs <= mcsMax_; ++mcs)      // 2..27 for DL table 2
        if (table_->lookupBler({linkId_, mcsTableIndex_, mcs, snr}) <= targetBler_)  // exact ≤ (D-09)
            best = mcs;
    if (best < 0) return 0;                              // no usable MCS → CQI 0 (matches base "below quality")
    return mcsIndexToCqi(best);                          // D-02 bridge via NrAmc (see Open Q 1)
}
```

### Anti-Patterns to Avoid
- **Treating `inf` as a probability.** `PHYAbstraction` returns `inf` for MCS indices absent from the table (MCS 0/1 on DL table 2) and for out-of-grid inputs. Feeding `inf` into `uniform(0,1) ≤ BLER` makes *every* packet fail; feeding it into `BLER ≤ targetBler_` makes that MCS never selected (often desired) — but it must be detected and handled explicitly, not stored verbatim.
- **Using `mcs_category=0` for DL.** That is PUSCH; DL table 2 under category 0 only covers MCS 9–27 (VERIFIED). DL = category 1.
- **Re-deriving EESM in C++.** Simu5G reads precomputed BLER only (D-07).
- **Selecting the feedback computation via ini typename.** It is built by a hardcoded C++ factory, not a NED `@class` typename (see Pitfall 1).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| effective-SINR→BLER for 5G-NR | Custom EESM/MIESM + LDPC curves | `sionna.sys.PHYAbstraction` (EESM default) | Ships calibrated 38.211/212/214 reference tables; correct MCS/TB handling. `[VERIFIED: venv]` |
| CQI↔(mod, coderate) mapping | New CQI table | `NrAmc::getMcsElemPerCqi` + `NrMcsTable` | The live 256-QAM extended tables already exist (`NrMcs.cc:24`). `[VERIFIED: codebase]` |
| Manifest JSON parse | New parser | in-tree `nlohmann/json` 3.9.1 | Reused in Phase 1; zero default-build symbols. `[VERIFIED: codebase]` |
| Reception draw + HARQ heuristic | New error model | inherited `isReceptionSuccessful` body (`:1801-1838`) | Only the BLER *source* changes (MOD-03); the per-RB success product, `harqReduction_`, and `uniform(0,1)` draw stay verbatim. `[VERIFIED: codebase]` |
| CQI argmin-vs-targetBler loop | New selection logic | structure of base `getCqi` (`:96-105`) | The base already iterates MCS/CQI choosing closest-to-targetBler; the subclass swaps the BLER source and the "highest ≤ target" rule. `[VERIFIED: codebase]` |

**Key insight:** Almost everything Phase 2 needs already exists on *one* of the two sides (calibrated BLER in Sionna; CQI/MCS tables + reception/feedback machinery in Simu5G). The work is wiring a single shared table into two readers, not building models.

## Common Pitfalls

### Pitfall 1: The DL feedback computation is built by a hardcoded C++ factory, not a NED typename (FB-01)
**What goes wrong:** Assuming `SionnaFeedbackComputation` can be selected ini-only like `SionnaChannelModel` (SEAM-01). It cannot.
**Why it happens:** `LtePhyEnb::initializeFeedbackComputation()` (`LtePhyEnb.cc:348`) does `new LteFeedbackComputationRealistic(binder_, targetBler, numBands)` directly; `getFeedbackComputationFromName` (`:320`) only knows the string `"REAL"`. `LteDlFeedbackGenerator::getFeedbackComputationFromName` (`:222`) just sets `feedbackComputationPisa_ = true`. There is no `@class`/typename indirection.
**How to avoid:** FB-01 must change the factory: either (a) add a new name (e.g. `"SIONNA"`) selected by a `LtePhyEnb` NED param, or (b) gate on whether the active channel model is `SionnaChannelModel`/a `SionnaManager` is present, and instantiate `SionnaFeedbackComputation`. Pass it the `SionnaTable` + `targetBler_` + the MCS-table info. **Decide this in planning (Open Q 2).**
**Warning signs:** A plan that says "select the feedback computation via ini" with no factory edit.

### Pitfall 2: DL category is 1 (PDSCH), MCS 0/1 are absent → `inf`
**What goes wrong:** Generating the table with category 0, or storing the `inf` for MCS 0/1 as if it were BLER.
**Why it happens:** Sionna's `MCSDecoderNR` defines `0=PUSCH, 1=PDSCH` (verified docstring), and the shipped PDSCH table 2 starts at MCS 2; lower indices and out-of-grid points return `inf`.
**How to avoid:** Use `mcs_category=1`. Record `mcs_min=2, mcs_max=27` (`num_mcs=26`) in the manifest. Have `lookupBler` reject `mcsIndex < mcs_min` (a CQI that maps below MCS 2 → CQI 0 / no-reception, matching the existing `cqi==0 → return false` reception guard at `:1781`). Add a tool-side assertion that no stored BLER is non-finite.
**Warning signs:** Any stored BLER == `inf`/NaN; a reception that fails 100% of packets for low CQI.

### Pitfall 3: Top 256-QAM MCS (26/27) has a residual BLER floor even at ~70 dB
**What goes wrong:** Expecting every MCS to be reachable (BLER 0) in the empty world, so the self-check picks MCS 27.
**Why it happens:** The shipped table's SINR grid maxes at 30 dB; beyond it BLER clamps to the 30 dB value. VERIFIED at 70 dB: MCS 26 → 0.00067, **MCS 27 → 0.150** (> `targetBler_=0.01`). So `getCqi` will select **MCS 26**, not 27, and the D-08 self-check must expect that (it is still ≤ target for 26).
**How to avoid:** The self-check asserts `BLER(selected MCS) ≤ targetBler_`, which holds for the *selected* MCS by construction — do **not** additionally assert that the *top* MCS is reachable. Document the expected selected MCS (likely 26) in the test so a regression is visible.
**Warning signs:** A self-check or test that hard-codes "selected MCS == 27".

### Pitfall 4: The reception path keys on CQI, not MCS index (D-01/D-02 bridge)
**What goes wrong:** `lookupBler` is keyed by MCS index 0–27, but `isReceptionSuccessful` and the on-the-wire `UserTxParams` only carry a **CQI 1–15** (`:1726`). There is no MCS-index field at reception.
**Why it happens:** Simu5G's `NrMcsElem` holds `(mod_, coderate_)` only — there is no public CQI→MCS-index function returning 0–27 (verified: `getMcsElemPerCqi` returns an element, not an index).
**How to avoid:** Establish an explicit, single CQI↔MCS-index bridge (Open Q 1). Recommended: at reception, map the CQI to a representative MCS index via `NrAmc` (CQI→mod,coderate→the matching `NrMcsTable::table[]` row index), and look up BLER for that MCS. The feedback side does the inverse (selected MCS index → CQI). Both must use the **same** bridge so reception and feedback agree (the whole point of FB-01).
**Warning signs:** Two different CQI↔MCS mappings in the reception and feedback paths.

### Pitfall 5: `harqReduction_` interaction (boundary with V2-07)
**What goes wrong:** Re-deriving HARQ gain from Sionna or dropping the `harqReduction_` multiply.
**Why it happens:** The reception code multiplies `per * pow(harqReduction_, attempt-1)` (`:1817`/`:1969`). Per-RV HARQ precompute is V2-07 (deferred).
**How to avoid:** Keep `harqReduction_` exactly as-is on top of the Sionna BLER (CONTEXT D scope). Only the first-transmission BLER comes from the table.

### Pitfall 6: Fingerprint/determinism
**What goes wrong:** A new BLER source shifts fingerprints silently, or float nondeterminism leaks.
**Why it happens:** Phase 1 pinned `tests/fingerprint/sionna_singlelink.csv` for the path-gain-only config. Adding BLER changes reception outcomes → the fingerprint will change and must be re-pinned (REP-01 intent).
**How to avoid:** Re-capture the Sionna config fingerprint via `fingerprinttest.py` after the BLER wiring lands (its own baseline, never matched to the analytic model). PHYAbstraction itself is **deterministic** (VERIFIED: identical BLER across repeated calls) and runs offline, so the committed `bler.bin` is the reproducible source of truth — pin it like `path_gain.bin`.

## Runtime State Inventory

> Not a rename/refactor/migration phase. This is additive feature work (new table dimension + two new readers). Section included for completeness; no pre-existing runtime state is renamed or migrated.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | The committed Phase-1 artifact (`path_gain.bin` + `manifest.json`) is path-gain-only; Phase 2 regenerates it to add `bler.bin` + new manifest fields. | Regenerate artifact via `precompute.py`; re-pin. Not a rename — additive schema. |
| Live service config | None — no external services. | None — verified (offline tool + in-process C++ only). |
| OS-registered state | None — no OS-level registrations. | None — verified. |
| Secrets/env vars | None. | None — verified. |
| Build artifacts | `tests/fingerprint/sionna_singlelink.csv` (Phase-1 baseline) becomes stale once reception consumes BLER. | Re-capture the fingerprint baseline after wiring (Pitfall 6). |

## Code Examples

### Offline: full per-MCS BLER stage (verified call shape)
```python
# Source: VERIFIED live in /home/zoli/Projects/OMNET/Sionna/venv (sionna 2.0.1)
import numpy as np, torch
from sionna.sys import PHYAbstraction

def compute_bler_table_dl(eff_sinr_linear_per_link, mcs_min=2, mcs_max=27,
                          mcs_table_index=2, mcs_category=1):
    """Return bler[L, M] over MCS mcs_min..mcs_max for each link's effective SINR.
    DL = PDSCH = category 1; table 2 = 256-QAM extended (MCS 2..27)."""
    phy = PHYAbstraction()                       # EESM default; shipped tables
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
            b = float(np.asarray(r[4]).reshape(-1)[0])   # r[4] = bler
            if not np.isfinite(b):
                raise ValueError(f"non-finite BLER for link {li} mcs {mcs}: {b}")
            out[li, mi] = b
    return out, mcs_list
```

### Reception swap (MOD-03), inside `isReceptionSuccessful`
```cpp
// Source: LteRealisticChannelModel.cc:1790-1817 — only the BLER SOURCE changes.
// BEFORE: blockErrorRate = binder_->phyPisaData.getBler(itxmode, cqi, snr);
// AFTER (Sionna):
int mcs = cqiToMcsIndex(cqi, DL);                         // D-02 bridge (Open Q 1)
double blockErrorRate = sionnaTable_->lookupBler({linkId, mcsTableIndex_, mcs, (double)snr});
// ... unchanged: blockSuccessRate, pow(.., allocation), cumulativeSuccessProbability,
//     packetErrorRate, effectiveErrorRateWithHarq = per * pow(harqReduction_, attempt-1),
//     receptionFailed = uniform(0,1) <= effectiveErrorRateWithHarq;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| TF-era Sionna `sionna.channel` BLER chains | `sionna.sys.PHYAbstraction` (PyTorch, calibrated tables) | Sionna 2.x (2025+) | The supported, TF-free path; training-data TF examples do not run. `[VERIFIED: venv]` |
| Simu5G CQI-keyed `phyPisaData.getBler` | MCS-keyed `SionnaTable::lookupBler` | This phase | One table, two readers; provable MCS/BLER agreement. |

**Deprecated/outdated:**
- Any `mcs_table`-as-`category-0`-for-DL assumption: WRONG; DL is category 1 (verified).
- Sionna 0.x/1.x `sn.rt`/TF objects: not installed.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `num_allocated_re=16800` (≈100 RB slot) is representative; the BLER value is governed by effective SINR + MCS, with weak RE-count dependence at v1 fidelity | Code Examples | The TB/code-block sizing shifts BLER slightly; pin the RE count from the actual scenario bandwidth for fidelity. Tool-side test should sweep a realistic value. |
| A2 | The single empty-world effective SINR (noise-limited, ~70 dB from Phase 1) is the correct x-input; EESM≈MIESM there | D-07 / Summary | If the chosen SINR convention differs from Simu5G's `getSINR` value, the BLER curve is offset; the x-axis contract (post-eq SINR) must match on both sides. |
| A3 | The CQI↔MCS-index bridge can be realized via `NrAmc::getMcsElemPerCqi` + a reverse lookup into `NrMcsTable::table[]` | Pitfall 4 / Open Q 1 | If no clean reverse map exists, a small explicit CQI→MCS-index table (38.214 derived) is needed; must be identical in both readers. |
| A4 | `targetBler_` for DL feedback is the channel-model `targetBler` (0.01); note `LtePhyEnb.ned` separately defaults `targetBler=0.001` and the factory default is 0.1 | D-09 / Open Q 3 | If the feedback object is constructed with `LtePhyEnb`'s `targetBler` (0.001) rather than the channel model's (0.01), the self-check's "single source" premise breaks. Must verify which `targetBler` the Sionna feedback computation receives. |
| A5 | DL table 2 valid MCS range is exactly 2–27 (`num_mcs=26`) | Pitfall 2 | If a future Sionna minor version reshapes the shipped table, the range assertion catches it (fail-loud). |

**If this table is empty:** it is not — these five need confirmation in discuss/plan before locking.

## Open Questions

1. **CQI↔MCS-index bridge (D-01/D-02 realization).** Reception and the wire carry CQI 1–15; the Sionna table is keyed by MCS index 0–27; Simu5G's `NrMcsElem` stores `(mod,coderate)`, not an index.
   - What we know: `NrAmc::getMcsElemPerCqi(cqi, DL)` (`NrAmc.cc:241`) deterministically maps CQI→(mod,coderate) by searching `NrMcsTable::table[]` (the 256-QAM extended rows). The row index *is* the MCS index.
   - What's unclear: whether to (a) add a small public `cqiToMcsIndex`/`mcsIndexToCqi` pair near `NrAmc`, reused by both readers, or (b) have the feedback side emit the MCS index and the scheduler/reception keep it.
   - **Recommendation:** Add one shared `cqiToMcsIndex(cqi,dir)` (returns the `NrMcsTable::table[]` index whose `(mod,coderate)` matches the CQI's element) + its inverse, used identically by `SionnaChannelModel` reception and `SionnaFeedbackComputation`. This is the single mapping that makes "MCS the scheduler assigned == MCS whose BLER is realized" literally true.

2. **How to select `SionnaFeedbackComputation` (FB-01 wiring).** The feedback computation is built by a hardcoded factory (`LtePhyEnb.cc:348`, `:320`), not a NED typename.
   - What we know: `getFeedbackComputationFromName` only handles `"REAL"`; `LteDlFeedbackGenerator` mirrors it.
   - What's unclear: add a new name/NED param on `LtePhyEnb`, or auto-detect the active `SionnaManager`/`SionnaChannelModel` and switch.
   - **Recommendation:** Gate on the presence of a configured `SionnaManager` (the same module the channel model resolves) — if present, `initializeFeedbackComputation` builds `SionnaFeedbackComputation` with that table; else unchanged. Keeps SEAM-01's "opt-in via the channel model" spirit without a new user-facing knob. Confirm in planning.

3. **Which `targetBler` reaches the feedback computation (D-09 single-source).** Channel model default `0.01` (`LteRealisticChannelModel.ned:51`) vs `LtePhyEnb.ned:26` `targetBler=0.001` vs factory literal `0.1` (`LtePhyEnb.cc:325`).
   - **Recommendation:** Construct `SionnaFeedbackComputation` with the **channel model's** `targetBler_` (read from the active `SionnaChannelModel`) so the self-check, `getCqi`, and reception all share one value (D-09). Make the self-check read the exact same `double`. Verify no second `targetBler` silently diverges.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Sionna venv `/home/zoli/Projects/OMNET/Sionna/venv` | Offline BLER stage (TOOL-04) | ✓ | python 3.13.7 | — |
| `sionna` (sys) | `PHYAbstraction` | ✓ | 2.0.1 | — |
| `torch` | PHYAbstraction tensors | ✓ | 2.12.0 | — |
| shipped BLER tables `sionna/sys/bler_tables/PDSCH_table2.json` | DL 256-QAM BLER | ✓ | bundled | — |
| OMNeT++ (`opp_run`) + INET | C++ build + unit harness + fingerprint | ✓ (Phase 1 used it) | 6.4 | — |
| `pytest` (in venv) | offline tests | ✓ | 7.4.4 | — |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** none.

## Validation Architecture

> nyquist_validation is enabled (`workflow.nyquist_validation: true`).

### Test Framework
| Property | Value |
|----------|-------|
| Framework (offline) | pytest 7.4.4 in the Sionna venv; marker `requires_venv` (`tools/sionna_precompute/pytest.ini`) |
| Framework (C++) | standalone harness `tests/sionna/unit/run_unit_tests.sh` (liboppsim/libINET, no kernel) |
| Config file | `tools/sionna_precompute/pytest.ini` |
| Quick run (offline) | `/home/zoli/Projects/OMNET/Sionna/venv/bin/python -m pytest tools/sionna_precompute/tests -q` |
| Quick run (C++ unit) | `tests/sionna/unit/run_unit_tests.sh` |
| Phase gate | feature-ON build + Sionna single-link config run + re-pinned fingerprint |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TOOL-04 | BLER[L,M] monotone in MCS, deterministic, finite for MCS 2–27 (DL) | unit (pytest) | `python -m pytest tools/sionna_precompute/tests/test_bler_table.py -x` | ❌ Wave 0 |
| TOOL-04 | `inf` rejected for MCS 0/1; tool raises | unit (pytest) | same file | ❌ Wave 0 |
| ART-02 | manifest carries `mcs_table_index`, `mcs_category`, `num_mcs`, `sinr_effective_fun`; `bler.bin` size == `L*num_mcs*8` | unit (pytest + C++) | pytest artifact test + `test_bler_lookup.cc` | ❌ Wave 0 |
| D-05/D-06 | `lookupBler` bounds-checks (bad link/mcs/table-index → cRuntimeError) | unit (C++) | `tests/sionna/unit/run_unit_tests.sh` | ❌ Wave 0 (extend) |
| D-02 | `cqiToMcsIndex`/inverse round-trips; identical in both readers | unit (C++) | `test_bler_lookup.cc` | ❌ Wave 0 |
| FB-01 / D-08 | self-consistency: selected-MCS BLER ≤ targetBler_ for the link | unit (C++) + sim | `test_bler_lookup.cc` (pure check) + feature-ON run | ❌ Wave 0 |
| MOD-03 | reception uses Sionna BLER + keeps draw/harq; fingerprint stable & re-pinned | integration (fingerprint) | `fingerprinttest.py` on the Sionna config | ⚠️ re-pin existing |

### Sampling Rate
- **Per task commit:** the relevant quick command above (pytest subset or C++ harness).
- **Per wave merge:** full pytest (`requires_venv`) + full C++ harness.
- **Phase gate:** feature-ON build green, Sionna config runs, fingerprint re-pinned, full suites green before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tools/sionna_precompute/tests/test_bler_table.py` — covers TOOL-04 (monotonicity, determinism, inf-rejection, category=1 DL).
- [ ] `tests/sionna/unit/test_bler_lookup.cc` — covers D-05/D-06 lookup bounds, D-02 CQI↔MCS bridge, D-08 self-check pure logic.
- [ ] No new framework install (pytest + C++ harness already exist).

## Security Domain

> `security_enforcement: true`, ASVS level 1. Phase 2 adds no network/auth surface; it reads one more local artifact (`bler.bin`) and computes in-process. The Phase-1 input-validation controls extend directly.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth surface. |
| V3 Session Management | no | N/A. |
| V4 Access Control | no | N/A. |
| V5 Input Validation | **yes** | `bler.bin` size validated against `L*num_mcs*sizeof(double)` (mirror `SionnaTable::loadBinary` `:49`); manifest `mcs_table_index`/`num_mcs` typed-parsed + range-checked; reject non-finite BLER at load; `lookupBler` bounds-checks every key (D-06). |
| V6 Cryptography | no | `request_hash` remains provenance-only (not a security control), as in Phase 1. |

### Known Threat Patterns for {offline-tool + local-artifact C++ reader}
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Tampered/truncated `bler.bin` (over-read / DoS) | Tampering / DoS | Validate file size vs declared `[L,num_mcs]` before allocate; cap dims; bounds-checked lookup (extend Phase-1 `kMaxLinks` pattern). |
| Manifest declares mismatched `mcs_table_index`/`num_mcs` vs live AMC | Tampering | Extend `assertContractMatchesLiveScenario`; fail-loud on mismatch (CAL-02). |
| Non-finite BLER (`inf`/NaN) silently used | Tampering | Reject at tool write-time and at C++ load-time. |

## Sources

### Primary (HIGH confidence)
- **Installed venv introspection** `/home/zoli/Projects/OMNET/Sionna/venv` (sionna 2.0.1): `PHYAbstraction.__init__`/`__call__` signatures + docstring (Input/Output contract), live DL calls (category=1, table_index=2) producing BLER per MCS, determinism, `inf`-sentinel behavior, SINR grid edges, `MCSDecoderNR` category convention (0=PUSCH, 1=PDSCH), shipped `bler_tables/PDSCH_table2.json` MCS-key range 2–27. `[VERIFIED: venv introspection]`
- **Simu5G source tree** (this session): `LteRealisticChannelModel.cc` `:1709/:1796/:1817/:1838/:1853/:1947/:1969`; `LteRealisticChannelModel.ned:51,53`; `LteFeedbackComputationRealistic.{h,cc}` `:24,:83,:96-105`; `LtePhyEnb.cc:225-353`; `LteDlFeedbackGenerator.cc:222`; `NrMcs.{h,cc}` `:24,:124,:145`; `NrAmc.{h,cc}` `:241`; `SionnaTable/SionnaManager/ManifestReader/SionnaChannelModel` (Phase-1 files). `[VERIFIED: codebase grep + read]`
- **Phase-1 artifacts:** `01-01..01-04-SUMMARY.md`, `precompute.py`, `scenario.example.json`. `[VERIFIED: codebase]`

### Secondary (MEDIUM confidence)
- CONTEXT.md D-01..D-09 (user-locked decisions, copied verbatim above). `[CITED: 02-CONTEXT.md]`
- CLAUDE.md stack table (Sionna 2.x = Dr.Jit/Mitsuba + PyTorch; PHYAbstraction; fail-loud). `[CITED: CLAUDE.md]`

### Tertiary (LOW confidence)
- None — all load-bearing claims were verified against the venv or the source tree this session.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — packages verified live in venv; no new installs.
- Architecture (offline BLER stage): HIGH — call shape executed and outputs confirmed.
- Architecture (C++ reception/feedback seams): HIGH — exact files/lines read; factory mechanism confirmed.
- CQI↔MCS-index bridge: MEDIUM — mechanism identified (`getMcsElemPerCqi`), exact reverse-map helper left to planning (Open Q 1).
- targetBler single-source: MEDIUM — three different `targetBler` defaults exist; the one feeding the Sionna feedback computation must be confirmed (Open Q 3 / A4).

**Research date:** 2026-06-18
**Valid until:** 2026-07-18 (stable; pinned venv + in-tree source). Re-verify if the Sionna venv is upgraded (would change shipped tables and could invalidate the MCS-range/category findings).
