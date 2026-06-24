# Plan B — Sionna BLER Curves (link-level library)

Status: design discussion / draft
Parent: [Sionna PHY Integration — Overview](sionna-integration-plan.md)
Sibling: [Plan A — Sionna channel](sionna-channel-plan.md)

Scope: an optional, **geometry-independent** `SINR → BLER` curve library, as a
pluggable replacement for Simu5G's existing `PhyPisaData`. This plan is fully
independent of Plan A — it shares no data with the channel, only the `ICurveProvider`
seam through which Simu5G consumes whatever SINR it computed.

---

## 1. Goal & boundary

Provide `BLER(SINR, MCS)` curves for the link-level mapping that turns an SINR into a
block error probability. Default provider stays Simu5G's `PhyPisaData`; a
Sionna-generated curve library is an optional **accuracy** upgrade (NR LDPC vs. the
LTE-era turbo/MCS that PhyPisaData encodes).

Boundary: this plan **consumes** the per-RB SINR that Simu5G forms (from Plan A or
from any channel model) and returns BLER. It knows nothing about geometry.

## 2. Why the curves are geometry-independent

`BLER(SINR, MCS)` depends on modulation/coding/receiver and on the channel only as a
statistical *class* — never on the specific link. Its real dimensions are modest:

- **MCS** (required), **numerology/SCS**, **TB-size / allocation bucket**.
- **NOT carrier frequency** (that affects propagation, i.e. Plan A — not the decode
  behavior), and **NOT geometry**.

So B is a **one-time, reusable library**, not a per-scenario computation. This is
what lets A and B be separate plans, and what kills any geometry × MCS blow-up: you
precompute the channel (A), and BLER is a runtime lookup into a geometry-free table.

## 3. Obtaining the curves: bundled vs on-demand

- **(a) Bundled, default**: precompute the full grid **once, offline**, and ship it
  with Simu5G as data files (the analog of how `PhyPisaData` is already baked in).
  Deterministic; no runtime Python/GPU.
- **(b) On-demand, fallback**: a `SionnaCurveProvider` spawns Sionna to generate a
  curve only on a **cache miss** (config outside the bundled grid), writing it back
  so it is a one-time cost.

The library is keyed by MCS / numerology / TB-size — **not** geometry — so it is
reused across all scenarios.

### Sizing (why (a) is the default)

Dimensions: MCS (15–29) × numerology (1–4) × TB-size bucket (1–8), AWGN/SISO in v1.

- ~**15–29 curves** (v1) up to ~**1,000** (generous, all dims).
- ~**1–4 KB/curve** in CSV ⇒ **well under 1 MB for v1, single-digit MB for a full
  library** (tens of MB only if MIMO ranks + HARQ RVs are added).
- For comparison, `PhyPisaData` is ~**17 KB** compiled in.

The real cost is **compute, not storage**: each curve is a link-level Monte Carlo
(~10⁴–10⁵ block decodes per SINR point to resolve low BLER), i.e. minutes–hours of
one-time GPU work for the whole grid — which is exactly why precompute-once (a) beats
lazy generation, leaving (b) as a niche cache-filler.

## 4. The `ICurveProvider` seam

- Abstract the `getBler()` call behind a small `ICurveProvider` interface.
- Implementations:
  - `PhyPisaDataCurveProvider` — current default (LTE-era AWGN/TU tables).
  - `BundledCurveProvider` — a precomputed Sionna grid shipped as data files.
  - `SionnaCurveProvider` — optional, generates a curve on a cache miss.
- The channel model is unaffected by which is active.
- **Feedback consistency**: `LteFeedbackComputationRealistic::getCqi()` also uses
  `PhyPisaData::getBler()` today, so it stays consistent with reception automatically
  in v1. When B switches to a Sionna provider, route feedback through the **same**
  `ICurveProvider` so the MCS the scheduler picks and the BLER it realizes agree. ⇒
  the library must contain curves for **all** CQIs/MCS.

## 5. Curve type & the bridge (correctness)

- **v1 = per-RB SINR (from Plan A) + AWGN curves + Simu5G's existing per-RB product**
  (`∏ blockSuccessRate^allocation`). Frequency selectivity is carried by the spread
  of per-RB SINRs, so the per-RB curve is the **AWGN** one. Do **not** use a
  fading/TU curve here — the ray-traced channel already encodes the fades
  (double-count).
- **Wideband** (one SINR per TB) instead requires either a channel-type (TU-like)
  curve **or** an EESM/MIESM effective-SINR compression. EESM is the cleaner modern
  bridge and makes a single AWGN library rigorous regardless of selectivity, but it
  is a step Simu5G does not have today.

## 6. What "generate curves" runs in Sionna

- Full **link-level Monte Carlo**: real PUSCH/PDSCH LDPC chain over the channel,
  BLER by counting. Highest fidelity.
- Cheaper: channel → post-equalization **effective SINR → BLER** via reference
  curves.
- Either way the output is a curve set; statistical confidence at low BLER (≤1e-2)
  drives the per-point batch size.

## 7. Determinism & fingerprints

- The curve library is a frozen artifact; lookups are deterministic. Pin/commit it.
- `PhyPisaData` is already deterministic. Switching providers changes BLER values
  (hence needs new fingerprint baselines) but not RNG-stream consumption (the success
  draw is unchanged).

## 8. Open questions (curve-specific)

1. **Provider for v1** — keep `PhyPisaData`, or generate a Sionna NR-LDPC library?
   (Accuracy question, orthogonal to geometry.)
2. **Grid dimensions** — TB-size bucketing, numerology coverage, MCS table(s).
3. **Curve type** — confirm AWGN for the decoupled per-RB design; map onto
   `PhyPisaData`'s TX-mode rows.
4. **EESM/MIESM adoption** — needed for wideband / AWGN-curve rigor; defer?
5. **HARQ** — keep Simu5G's `harqReduction_` heuristic vs. per-redundancy-version
   curves.
6. **`ICurveProvider` shape** — exact interface wrapping `getBler()`.
7. **Format** — CSV (human-readable) vs. binary; versioning; numerology/MCS-table tag.

## 9. Relevant code (anchors)

- `src/simu5g/common/blerCurves/PhyPisaData.h` — `getBler(txMode, cqi, snr)`,
  3×15×49 AWGN/TU tables. Retained as the default provider; wrapped by
  `ICurveProvider`.
- `src/simu5g/common/blerCurves/BLERvsSINR_15CQI_{AWGN,TU}.h` — the curve tables.
- `src/simu5g/stack/phy/feedback/LteFeedbackComputationRealistic.{h,cc}` —
  `getCqi()` inverts the BLER table to select MCS; must use the same provider.
- `src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc` —
  `isReceptionSuccessful()`: the per-RB product + HARQ + success draw that consumes
  `getBler()`.
