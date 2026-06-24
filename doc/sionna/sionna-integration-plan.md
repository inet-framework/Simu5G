# Sionna PHY Integration — Overview

Status: design discussion / draft
Scope: optional integration of NVIDIA Sionna into Simu5G for site-specific PHY
modeling, initially for **completely static scenarios**.

This is the umbrella document. The work is split into **two independent plans**
because the two components share no data and can be built, shipped, and used
separately:

- **[Plan A — Sionna channel (ray-traced attenuation)](sionna-channel-plan.md)** —
  geometry-specific propagation; the part that genuinely needs Sionna RT.
- **[Plan B — Sionna BLER curves (link-level library)](sionna-blercurves-plan.md)** —
  geometry-*independent* `SINR → BLER` curves; pluggable, default = existing
  `PhyPisaData`.

---

## 1. Goal

Let Simu5G *optionally* use Sionna for more accurate, site-specific PHY modeling,
without affecting the default build/behavior. Sionna is opt-in (NED polymorphism
`like ILteChannelModel` + an ini setting); no Python/TensorFlow/GPU dependency is
added to a normal build or run.

## 2. Division of labor

| Concern | Nature | Owner | Plan |
|---|---|---|---|
| Per-(link, RB) attenuation / path gain (all Tx–Rx pairs, incl. interferers) | **Static** (geometry) | **Sionna RT** | A |
| `BLER(SINR, MCS)` curve library | **Geometry-independent**, one-time | **Curve provider** (PhyPisaData default / Sionna optional) | B |
| SINR aggregation (Σ desired + interferers + noise) | **Runtime** | **Simu5G** | — |
| per-RB SINR → block error + success draw + HARQ | Runtime | **Simu5G** | — |

The two plans are independent because the curve library depends only on
MCS/numerology/TB-size — **never on geometry or carrier frequency** — so it is a
reusable artifact, not a per-scenario computation. You can adopt A with B left on
`PhyPisaData`, or improve B independently of any geometry work.

## 3. The interface where A and B meet (the contract)

Simu5G is the glue and owns the seam:

1. **Plan A** delivers per-(link, RB) attenuation (a power-independent multiplier).
2. **Simu5G** forms per-RB SINR =
   `(Tx power × attenuation) / (Σ interferer Tx powers × attenuations active this TTI + thermal noise·NF)`.
3. **Plan B** maps that SINR → BLER via `ICurveProvider`, then Simu5G does the
   per-RB product + HARQ + success draw.

The one consistency rule that spans both plans: **the SINR definition Simu5G feeds
must match the x-axis of B's curves.** v1 uses **per-RB SINR + AWGN curves**: the
frequency selectivity is carried by the spread of per-RB SINRs (from A), each RB is
≈ flat, so the per-RB curve is the AWGN one — do **not** also use a fading/TU curve
(double-counting). Wideband instead of per-RB would require EESM/MIESM or a
channel-type curve. See each plan for detail.

## 4. Phasing (across both plans)

- **v1**: Plan A (Sionna per-RB channel) + Plan B = `PhyPisaData` AWGN curves +
  Simu5G's existing per-RB-product bridge. Start noise-limited, then add
  interference. Site-specific propagation immediately, zero curve-generation cost.
- **later**: Plan B Sionna NR-LDPC curve library; EESM bridge; per-RB MIMO + rank
  adaptation; Doppler / mobility / live coupling.

## 5. Determinism & fingerprints (spans both)

- Both the channel table (A) and the curve library (B) are **frozen artifacts**; the
  only runtime randomness is the existing success draw under the Simu5G RNG ⇒ runs
  are reproducible given the artifacts.
- Pin/commit both so baselines are stable regardless of GPU/TF float
  nondeterminism. (`PhyPisaData` is already deterministic.)
- Sionna configs need their **own** fingerprint baselines: RNG-stream consumption
  differs from the statistical model (no per-link fading draws).
