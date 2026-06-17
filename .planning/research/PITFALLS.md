# Pitfalls Research

**Domain:** NVIDIA Sionna RT → Simu5G (OMNeT++/INET C++) ray-tracing-to-system-level link abstraction (precompute-once link-to-system, static scenarios)
**Researched:** 2026-06-17
**Confidence:** HIGH on the structural pitfalls (units/convention/SINR-definition/double-counting/determinism — these follow directly from Sionna's documented path-coefficient math and Simu5G's verified channel pipeline); MEDIUM on exact calibration tolerances and effective-SINR validity bounds (scenario-dependent, must be measured, not assumed).

This file targets the mistakes that specifically wreck an RT-to-system-level integration. Generic software advice is omitted. Every pitfall below is "silent": the simulation keeps running and the numbers look plausible, which is exactly why each one needs an explicit, automated guard rather than eyeballing.

---

## Critical Pitfalls

### Pitfall 1: Silent coordinate / units / scale mismatch between OMNeT++ playground and the Sionna scene

**What goes wrong:**
OMNeT++ node positions (meters, z-up, playground origin at a corner) are fed into a Sionna scene whose origin, axis convention, or unit differs. Common concrete failures: Blender/Mitsuba scenes that are y-up or z-into-screen; scenes exported in centimeters or scene-units instead of meters; an origin offset because the OSM/Blender mesh is centered on its own bounding box, not the OMNeT++ origin; antenna height applied in the wrong axis so a 25 m gNB ends up at x=25 m, height 0. The ray tracer still finds paths, still returns finite path gains, BLER still comes out in [0,1] — the result is wrong but *plausible*.

**Why it happens:**
There are two authoritative geometries (OMNeT++ coords vs. the Sionna mesh) and the transform between them is implicit. Sionna RT bakes the free-space factor `λ/(4π)` and the full geometric path into the path coefficient `a_n = (λ/4π)·C_R·(∏Tₙ)·C_T`, so a position error shows up only as a smoothly-shifted path gain, never as an exception. Nobody notices a gNB that is 10 m too low or a UE on the wrong side of a wall.

**How to avoid:**
- One **shared scenario source** (single description: node positions, antenna heights, orientations, carrier, numerology, band count) generates *both* the OMNeT++ ini/positions and the Sionna scene placement. Never hand-place nodes twice.
- Make the OMNeT++↔Sionna transform an **explicit, named, version-pinned object** (origin offset, axis permutation/handedness, scale), not scattered constants.
- **Round-trip assertion at init:** for a handful of links, compute free-space loss from the OMNeT++ Euclidean distance and compare to Sionna's LOS-only path gain for the same pair; they must agree to within a tight bound in the empty world. A position/scale bug fails this immediately.
- Render the Sionna scene with the node markers overlaid and eyeball it once per scene (a cheap human check that catches gross transforms).

**Warning signs:**
RSRP/path gain that is suspiciously uniform across links of very different distances; LOS where geometry says NLOS (or vice versa); empty-world Sionna path gain off from free-space by a constant ratio (= scale error) or a constant dB offset; a UE that "should" be blocked by a building showing strong LOS.

**Phase to address:**
Phase: **Geometry & coordinate contract** (the empty-world calibration phase). This is the first thing the reference/empty-world mode must prove, before any site-specific scene is trusted.

---

### Pitfall 2: Tx-power / path-gain convention mismatch (dB vs linear, where Tx power and antenna gain live)

**What goes wrong:**
Sionna's channel gain `g = Σ|aₙ|²` is a **linear power gain** that already includes the transmit and receive **antenna patterns** `C_T`, `C_R` but does **not** include the transmitter's radiated **power** (it is per-unit-input). Simu5G, by contrast, carries Tx power in dBm and applies antenna gain and attenuation as separate dB terms in `getRSRP()`/`getSINR()`. The integration mixes these up: applies Sionna's gain in dB when it is linear, double-applies antenna gain (once inside Sionna's pattern, once via Simu5G's antenna-gain term), or adds Tx power on one side but not the other. Received power lands 10–40 dB off, BLER saturates to 0 or 1, and CQI feedback locks to the top or bottom MCS — but the sim runs.

**Why it happens:**
The two tools split the link budget differently. Sionna folds antenna pattern + geometry into one complex coefficient; Simu5G keeps Tx power, antenna gain, and path loss as separable dB scalars. The boundary "what does the cached number represent" is easy to leave undocumented, and dB↔linear (and dBm↔dBW↔W) errors are exactly 10·log10 factors that still look like reasonable powers.

**How to avoid:**
- Write a **one-line link-budget contract** in the cache schema header: e.g. "stored value = 10·log10(Σ|aₙ|²) in dB, antenna patterns included, Tx power NOT included, reference impedance/normalization = X." Both the Sionna exporter and the C++ loader assert against this string.
- Decide once **who owns antenna gain**. If Sionna's `C_T`/`C_R` patterns already model the array gain, Simu5G must **not** re-add its antenna-gain term for Sionna-active links. Pick one and disable the other in code, with a comment pointing at this pitfall.
- **Empty-world numeric check:** received power = Tx power (dBm) + Sionna path gain (dB) [+ any agreed antenna term] must reproduce the textbook Friis budget for a LOS pair to within ~0.5–1 dB. This single test catches dB/linear, dBm/dBW, and double-antenna-gain bugs.

**Warning signs:**
BLER that is all-0 or all-1 across the whole scenario; CQI feedback pinned to MCS 0 or the maximum for every UE; RSRP off by a clean ~30 dB (W↔mW), ~10/20 dB (gain double-count), or a smooth factor (linear-as-dB); path gain that looks like a small number near 1 being treated as dB.

**Phase to address:**
Phase: **Geometry & coordinate contract / empty-world calibration** (same phase as Pitfall 1 — both are validated by the empty-world Friis check). The contract string and the assert belong in the cache-format phase.

---

### Pitfall 3: SINR x-axis definition mismatch between the BLER curve and Simu5G's signal/(interference+noise)

**What goes wrong:**
Sionna generates the BLER-vs-SINR curve by sweeping some SINR-like quantity. If that quantity is **post-equalization effective SINR per stream**, or **average SNR of added white noise**, or **per-RB SNR**, it is *not* the same number as Simu5G's wideband `getSINR()` = desired / (Σ interference + thermal noise). Looking Simu5G's SINR up on a curve whose x-axis means something subtly different yields a BLER that is off by an MCS or two everywhere — never crashes, just systematically wrong throughput.

**Why it happens:**
"SINR" is overloaded. The link-to-system literature distinguishes pre-detection SINR, per-stream post-MMSE SINR, effective (compressed) SINR, and geometry SNR. Sionna's link-level chain and Simu5G's interference summation were built by different people for different purposes, and the x-axis semantics are implicit in each.

**How to avoid:**
- **Define the SINR x-axis once, in writing, and make both sides honor it.** The plan already states the intended contract (white-noise average-SNR x-axis maps onto `signal/(interference+noise)`); encode that as the operative definition.
- If the curve is built by "add white noise at average SNR S to the site-specific channel, measure BLER," then the x-axis *is* the quantity Simu5G computes — keep it that. Avoid effective-SINR-per-subcarrier x-axes unless Simu5G's lookup feeds the *same* compressed quantity.
- **Cross-check at a known point:** for a noise-limited LOS link, the SINR Simu5G computes from Tx power + Sionna path gain + thermal noise must equal the x-axis value at which Sionna's curve was anchored for that link. Mismatch = wrong x-axis.
- For v1 (noise-limited, one BLER per link/MCS), this collapses to: the single SNR at which the BLER was baked must equal the SNR Simu5G derives at runtime. Assert it.

**Warning signs:**
Throughput consistently biased high or low by ~1–2 MCS steps vs. the analytic baseline in a comparable empty world; the BLER step (waterfall) sitting at an SNR that disagrees with the computed operating SNR by a fixed dB offset; curves whose 10%-BLER point is implausible for the chosen MCS on AWGN.

**Phase to address:**
Phase: **Link-to-system mapping / BLER pipeline** (the phase that defines the effective-SINR→reference-curve method). The x-axis contract is a gate for that phase.

---

### Pitfall 4: Double-counting deterministic RT path gain with Simu5G's statistical shadowing / LOS draw / penetration loss

**What goes wrong:**
Sionna's RT path gain *already* contains the site-specific shadowing, the deterministic LOS/NLOS condition, and the building-penetration effect (they are physical consequences of the geometry the rays traversed). If `SionnaChannelModel` inherits `LteRealisticChannelModel`'s `getSINR()` assembly and leaves the Gaussian shadowing process, the random LOS-vs-distance draw, or the 38.901 penetration-loss term active, those statistical effects are applied **on top** of the deterministic gain — counting the same physics twice. Result: extra random spread on a channel that should be deterministic, biased path loss, and broken determinism (the shadowing draw consumes RNG).

**Why it happens:**
`SionnaChannelModel : public LteRealisticChannelModel` reuses a `getSINR()` that was *designed* to add shadowing/fading/penetration. Override discipline is easy to get wrong — you replace the BLER lookup but forget that the same method also injects statistical path-loss terms upstream of where you hooked in.

**How to avoid:**
- Treat "**when Sionna is active it fully owns path gain**" as an invariant enforced in code, not a convention. The Sionna path must **not** call the shadowing process, the random LOS draw, or the penetration-loss term.
- Prefer overriding the *path-gain source* cleanly (return Sionna gain) rather than letting the inherited method add terms and trying to subtract them back out.
- **Determinism assertion:** with Sionna active, the only RNG consumption per reception should be the `uniform(0,1)` success draw (and HARQ heuristic). If the shadowing/fading RNG stream is touched, double-counting is present. Test by checking RNG-stream consumption counts.
- **Two-seed test:** run the static Sionna scenario with two different RNG seeds. Path gain / RSRP per link must be **identical** (deterministic geometry); only the success-draw outcomes may differ. Any RSRP variance between seeds proves a statistical term is leaking in.

**Warning signs:**
Per-link RSRP that varies run-to-run with the seed despite static geometry; a wider-than-expected SINR distribution; fingerprint instability traced to the shadowing/fading RNG substream being consumed; path loss systematically a few dB higher than Sionna alone reports.

**Phase to address:**
Phase: **SionnaChannelModel core (path-gain ownership)** — the phase that subclasses `LteChannelModel`/`LteRealisticChannelModel` and decides exactly which inherited terms are suppressed.

---

### Pitfall 5: CQI feedback and realized BLER computed from different tables (scheduler/PHY disagreement)

**What goes wrong:**
The scheduler picks MCS via the CQI feedback path (`LteFeedbackComputationRealistic::getCqi()` inverting a BLER table), but the realized reception uses the Sionna table. If feedback still uses the old `PhyPisaData` AWGN/TU table while reception uses Sionna curves, the scheduler chooses an MCS that the Sionna channel cannot actually sustain (or wastefully under-selects). Result: chronic BLER far above the 10% target, HARQ thrash, or throughput left on the table — all while each component looks internally consistent.

**Why it happens:**
There are two independent entry points into the BLER table (feedback/CQI computation and reception/`isReceptionSuccessful()`), and it is easy to rewire one and forget the other. The CQI table must also contain **all** CQIs/MCS per link, not just the operating one, or inversion is impossible.

**How to avoid:**
- Route **both** `getCqi()` and `isReceptionSuccessful()` through the **same** Sionna table object, keyed identically per link.
- The cached artifact must store BLER for **every CQI/MCS per link**, not only the selected MCS — required for the highest-MCS-with-BLER≤target inversion.
- **Self-consistency assertion:** for each link, the MCS the feedback path selects must have realized BLER ≤ `targetBler_` on the same table. Check this at init or as a logged invariant.

**Warning signs:**
Average realized BLER hovering well above the target (e.g. 30–50% instead of ~10%); excessive HARQ retransmissions; CQI reports that don't move when the Sionna channel clearly should change them; scheduler MCS that is constant while channel varies across links.

**Phase to address:**
Phase: **CQI/feedback rewiring** (depends on the table format from the precompute phase; should be a named phase distinct from the reception-path phase so both table consumers are covered).

---

### Pitfall 6: Per-link / per-RB collapse and "interference is white" treated as fidelity, not as a documented approximation

**What goes wrong:**
A transport block spans many RBs; collapsing the frequency-selective channel to **one curve per (link, MCS)** discards which RBs were actually allocated. On a strongly frequency-selective RT channel (deep notches from multipath), the wideband/effective figure can be optimistic or pessimistic versus the real allocation. Separately, treating interference as **white** (it shifts the operating SINR but does not reshape the curve) is exact only for interference-free or spatially-unstructured cases; an MMSE-IRC receiver exploiting interferer spatial structure is *not* captured by any precompute-once scheme. Mistaking these modeling choices for ground truth leads to over-claiming accuracy.

**Why it happens:**
The whole appeal of link-to-system is the collapse from per-subcarrier physics to a per-link scalar. The effective-SINR mapping (EESM/MIESM-style) is *valid only within its calibrated range*; pushed outside it (very high MCS, deep frequency selectivity, structured interference) the compression error grows silently.

**How to avoid:**
- **Document the approximation explicitly** in the model and the results: "v1 = per-link (not per-RB) effective-SINR mapping; interference treated white; exact only for noise-limited / interference-free links." (v1 is noise-limited, so the white-interference caveat is deferred to v2 — but write it now so v2 doesn't forget.)
- Keep v1 strictly **noise-limited** as scoped; that side-steps the white-interference error entirely for the first deliverable and isolates the per-RB-collapse question.
- When effective-SINR mapping is used, **validate the compression** against a few full per-RB / link-level Monte Carlo points to bound the error, rather than assuming the mapping holds everywhere.
- Be honest in any comparison: a per-link scalar cannot reproduce per-RB scheduling gains; don't compare against a metric that depends on them.

**Warning signs:**
Effective-SINR predictions that diverge from spot-check link-level points at high MCS or in deep frequency-selective channels; results being presented as "physically exact" when they rest on a wideband collapse; interference scenarios (v2) where IRC receivers are claimed but cannot be represented.

**Phase to address:**
Phase: **Link-to-system mapping / BLER pipeline** (same phase as Pitfall 3). The documented-approximation list is a deliverable of that phase; the validation spot-checks belong to the calibration phase.

---

### Pitfall 7: Determinism / fingerprint breakage from GPU/TensorFlow float non-determinism

**What goes wrong:**
The team commits a Sionna-derived table, expects byte-stable reruns, but the **table itself** changes between machines/GPU/driver/TF versions because TensorFlow GPU reductions are non-deterministic: float addition is non-associative and thousands of GPU threads sum in a nondeterministic order. Two precompute runs of the *same* scene yield slightly different path gains → different BLER → different MCS → different fingerprints. Or: the precompute is re-run as part of CI and the fingerprint baseline drifts every time.

**Why it happens:**
The C++/RNG side is deterministic, so people assume the whole pipeline is. The non-determinism lives entirely in the offline GPU stage and is invisible until two runs are byte-compared. (Confirmed: TF GPU reduction order is non-deterministic; `TF_DETERMINISTIC_OPS` / `enable_op_determinism` exist but some ops still vary or raise.)

**How to avoid:**
- **Decouple the table from the GPU run.** The committed, version-pinned cached artifact is the source of truth for reproducible simulations. The simulation loads the *pinned* table; it never regenerates it as part of a reproducible run. (Plan already states this — enforce it.)
- **Cache by request hash** so a rerun of the same scenario loads the committed table and never re-invokes Sionna; fingerprint stability comes from the artifact, not from re-tracing.
- For the precompute itself, if regeneration must be reproducible, set `TF_DETERMINISTIC_OPS=1` / `enable_op_determinism`, pin seeds, and pin Sionna/TF/CUDA versions — but treat this as best-effort and still **commit the artifact** as the real anchor.
- Give Sionna configs their **own fingerprint baselines** — they consume the RNG stream differently (no per-link fading draws), so they will never match the statistical model's baselines, and that is expected, not a regression.

**Warning signs:**
Fingerprints that differ across machines or GPUs for the "same" scenario; baseline drift each time the precompute is re-run; a teammate on a different CUDA/driver version unable to reproduce results; path gains that differ in the last few significant digits between two precompute runs.

**Phase to address:**
Phase: **Caching, artifact pinning & fingerprints** — the phase that defines the artifact format, the request-hash key, and creates the Sionna-specific fingerprint baselines. Must land before any fingerprint-tested scenario is committed.

---

### Pitfall 8: Expecting exact Sionna(empty-world) = Simu5G-3GPP agreement and lacking a defensible tolerance

**What goes wrong:**
The empty-world calibration is treated as a pass/fail equality test. It will "fail" — because Sionna RT in an obstacle-free world computes a (near) free-space / two-ray deterministic path, while Simu5G applies a **3GPP statistical** model (analytic path-loss formula + Gaussian shadowing + random LOS/NLOS draw + scalar penetration). These are *different physics by construction*; they cannot match to the dB. Either the team chases an impossible match (burning weeks tuning), or, worse, fudges Sionna parameters until it matches and thereby destroys the site-specific value.

**Why it happens:**
"Calibration" connotes "make them equal." But the 3GPP model is an *averaged statistical abstraction* and RT is a *deterministic instance*; the right comparison is "is the difference bounded and explainable," not "is it zero." The PROJECT.md explicitly flags this as a validation-honesty requirement.

**How to avoid:**
- **Reframe the empty-world test from equality to bounded explainable difference.** The goal is to catch *gross* unit/convention/coordinate bugs (Pitfalls 1–4), not to reproduce 3GPP statistics.
- Set the tolerance on the **deterministic, physics-shared** quantity: e.g. Sionna LOS path gain vs. **free-space (Friis)** for the same distance should agree tightly (~0.5–1 dB) — that is the same physics on both sides and a real bug detector. Do **not** set a tight tolerance against the 3GPP *formula* including shadowing.
- Compare distributions/trends, not single values: path gain vs. distance slope, LOS/NLOS ordering. Document the expected residual (3GPP excess loss / shadowing margin) so reviewers see it is intentional.
- Write down, before running, *what difference is acceptable and why* — a defensible tolerance is one derived from the known modeling-physics gap, not reverse-engineered to pass.

**Warning signs:**
Open-ended parameter tuning to "close the gap" with the 3GPP curve; Sionna material/scene parameters being altered to match Simu5G rather than reality; a calibration report that states "matches Simu5G" without quantifying or explaining the residual; passing the gate only by loosening tolerance until anything passes.

**Phase to address:**
Phase: **Empty-world / reference calibration** (its own named phase, gated on Pitfalls 1–4's contracts). The tolerance definition and rationale are deliverables of this phase and feed the validation-honesty note in PROJECT.md.

---

### Pitfall 9: Precompute cost blow-up (enumerating all links × MCS × CQIs × interferers)

**What goes wrong:**
Naïvely enumerating every (Tx, Rx) pair × every MCS/CQI × (in v2) every interferer combination × any sweep parameter makes the precompute combinatorial. With full link-level LDPC Monte Carlo per cell entry, a modest network's table balloons into days of GPU time, and the v2 interferer dimension multiplies it further. The team discovers this only when the first realistic scene is attempted and the offline tool never finishes.

**Why it happens:**
RT is cheap to *trace all pairs in one batch* but the **BLER generation per (link, MCS)** is the cost, and the CQI requirement ("table must hold BLER for all CQIs/MCS per link") forces the full MCS dimension. v1's "one BLER per link/MCS" is already #links × #MCS; v2's interferer dimension is the real explosion risk.

**How to avoid:**
- **Trace all Tx/Rx pairs in a single Sionna run** (the plan's global-precompute design) — RT batching is far cheaper than N independent calls and one process spawn.
- Choose the **cheaper internal BLER method for v1**: effective-SINR → reference curve rather than full per-entry LDPC Monte Carlo (the plan's chosen method — keep it). Reserve Monte Carlo for spot-check validation only.
- **Keep v1 noise-limited** so there is *no interferer dimension* in the table; that is the single biggest cost lever. Only add the interferer/SINR-curve axis in v2, and budget for it explicitly.
- Estimate the table size and GPU time **before** building the full enumerator (links × MCS × method cost) and put it in the precompute-phase plan.

**Warning signs:**
Precompute runtime scaling super-linearly with node count; the offline tool's memory/time growing when interferers are added; a table whose size is dominated by an axis (interferers, RBs) that v1 was supposed to exclude; "we'll just enumerate everything" with no size estimate.

**Phase to address:**
Phase: **Precompute / offline-tool design** (the enumerator and BLER-method choice). The size/time estimate is a gate before implementing the full enumerator.

---

### Pitfall 10: Cache-invalidation incorrectness — stale table silently used after an input changed

**What goes wrong:**
The cache is keyed by a hash of the request, but the hash omits an input that actually affects the result — a material property, antenna pattern file, Sionna/`max_depth` RT setting, frequency, Tx power, or the coordinate transform. The simulation loads a **stale** table that no longer matches the scenario, and (per the whole theme of this file) it runs and looks plausible. The opposite failure — the hash includes something irrelevant (a timestamp, absolute path) — defeats caching and re-invokes Sionna every run, killing the "invoke once" value and destabilizing fingerprints.

**Why it happens:**
Getting a cache key exactly right is hard: it must cover **every** input that changes the physics and **nothing** that doesn't. Materials, antenna patterns, and RT depth/accuracy settings are easy to forget because they live in the Sionna side, not the OMNeT++ ini.

**How to avoid:**
- Hash the **complete request**: scene geometry (mesh hash), per-surface materials, antenna arrays/patterns, positions/orientations, carrier freq, numerology/bandwidth/band count, Tx power, MCS set, RT settings (`max_depth`, sample counts), and the coordinate-transform object — exactly the list the plan names. Derive the hash from the *serialized canonical request*, not from individual scattered fields.
- **Exclude** non-physical inputs (timestamps, absolute paths, machine name) from the key so reruns hit the cache.
- **Fail loudly on mismatch:** the `SionnaChannelModel` asserts at init that the loaded table's embedded request-hash and its carrier/numerology/band-count metadata match the live scenario; on mismatch, **abort**, do not silently fall back to the analytic model or to the stale table.
- Store the full request *inside* the artifact so the loader can re-verify, not just trust the filename.

**Warning signs:**
Changing a material/antenna/`max_depth` and getting identical results (key missed an input); the precompute re-running on every launch despite an unchanged scenario (key too broad); fingerprint instability tied to cache regeneration; results that don't change when an input that physically should change them does.

**Phase to address:**
Phase: **Caching, artifact pinning & fingerprints** (same phase as Pitfall 7). The request-hash definition and the loud-mismatch assert are deliverables of that phase.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Subclass `LteRealisticChannelModel` and override only the BLER lookup | Fast to write; reuses interference summation | Inherited `getSINR()` keeps injecting shadowing/LOS/penetration → double-counting (Pitfall 4) | Only with an explicit audit that every statistical path-loss term is suppressed; otherwise prefer a clean path-gain override |
| Store only the operating-MCS BLER in the table | Smaller artifact | CQI inversion impossible → scheduler/PHY disagree (Pitfall 5) | Never — must store all CQIs/MCS per link |
| Hand-place nodes separately in OMNeT++ ini and Sionna scene | No tooling to build | Silent coordinate drift over time (Pitfall 1); diverges as scenarios edited | Only for a throwaway one-off scene, never for a validated scenario |
| Regenerate the table in CI for "freshness" | Always current | GPU float non-determinism drifts fingerprints every run (Pitfall 7) | Never for reproducible/fingerprinted runs — commit the pinned artifact |
| Loosen the empty-world tolerance until it passes | Green calibration gate | Hides real unit/convention bugs; destroys the bug-detection value of calibration (Pitfall 8) | Never — tolerance must be physics-derived, not pass-derived |
| Tune Sionna scene/material params to match the 3GPP curve | "Validated against Simu5G" | Throws away site-specific fidelity, which is the entire point of the project | Never |

## Integration Gotchas

Common mistakes when connecting Sionna (Python/TF/GPU) to Simu5G (C++/OMNeT++).

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Sionna path gain → Simu5G RSRP | Applying linear `Σ|aₙ|²` as if it were dB; double-counting antenna gain already in `C_T`/`C_R` | One documented link-budget convention string; assert it in loader; pick a single owner of antenna gain |
| OMNeT++ coords → Sionna scene | Implicit/duplicated geometry; wrong axis/scale/origin | Single shared scenario source + explicit versioned transform + empty-world Friis round-trip check |
| Offline tool → simulation | Sim regenerates the table at runtime (non-determinism, Python dependency creeps back in) | Separate offline tool produces a pinned artifact; sim only loads it; no runtime Python/TF/GPU |
| Cache key | Omitting materials / antenna patterns / RT depth; or including timestamps/paths | Hash the canonical serialized full request; exclude non-physical inputs; embed request in artifact for re-verify |
| Failure handling on mismatch | Silently fall back to analytic model or stale table | Fail loudly and abort at init on any contract/hash/metadata mismatch |

## Performance Traps

Patterns that work at small scale but fail as the scenario grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| N independent Sionna calls (one per link) | Long precompute; many process spawns | Trace all Tx/Rx pairs in one batched Sionna run | As soon as #links is more than a handful |
| Full LDPC Monte Carlo per (link, MCS) table entry | Precompute runs for hours/days | Effective-SINR → reference curve for v1; Monte Carlo only for spot-checks | Realistic node counts × full MCS set |
| Adding the interferer dimension to the precompute (v2) | Combinatorial table size/time blow-up | Keep v1 noise-limited (no interferer axis); budget v2 explicitly | The moment v2 enumerates interferer combinations |
| Per-RB enumeration of the channel | Table size explodes by #RBs | Per-link effective figure for v1 (documented approximation, Pitfall 6) | High-bandwidth / many-RB carriers |

## Security Mistakes

Not a security-sensitive domain (offline research tooling, no untrusted input, no network service). The closest analogues are **supply-chain / reproducibility integrity** of the cached artifact:

| Mistake | Risk | Prevention |
|---------|------|------------|
| Unversioned/unsigned cached table committed to the repo | Silent divergence if regenerated on a different TF/CUDA stack; irreproducible published results | Pin and commit the artifact with its embedded request hash + tool versions; treat it as the reproducibility anchor |
| Trusting a table whose embedded request doesn't match the live scenario | Wrong-but-plausible results published as validated | Loud init-time assert on embedded request hash + carrier/numerology/band metadata |

## UX Pitfalls

"Users" here are simulation researchers configuring the model via ini/NED.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Sionna model silently falls back to analytic on any error | Researcher believes they ran site-specific RT but got 3GPP statistics | Fail loudly; never silently substitute the other model |
| No visibility into which table/scene/hash a run used | Irreproducible, unexplainable results; "which scenario was this?" | Log the artifact hash, scene name, and contract string at init |
| Opt-in path changes default build/behavior | Breaks the core promise; existing users' fingerprints shift | Keep Sionna strictly opt-in (NED `like` + ini); default build has no Python/TF/GPU and is byte-for-byte unaffected |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Empty-world calibration:** often "passes" only because the tolerance was loosened — verify the tolerance is **physics-derived** (Friis vs. Sionna LOS, ~0.5–1 dB) and the 3GPP residual is *explained*, not just bounded.
- [ ] **Path-gain ownership:** BLER lookup replaced, but verify shadowing / random-LOS / penetration terms are **actually suppressed** (two-seed test gives identical RSRP).
- [ ] **CQI/feedback path:** reception uses Sionna table, but verify **feedback** (`getCqi`) uses the *same* table and that **all** CQIs/MCS per link are stored.
- [ ] **Link-budget convention:** path gain applied, but verify dB/linear and antenna-gain ownership via the empty-world Friis check (no clean ~10/20/30 dB offsets).
- [ ] **SINR x-axis:** curve looked up, but verify the x-axis definition matches `signal/(interference+noise)` at a known noise-limited point.
- [ ] **Cache key:** caching works, but verify it covers materials, antenna patterns, RT depth, and the transform — change each and confirm the result changes; change a timestamp and confirm it does *not* re-invoke.
- [ ] **Fingerprints:** runs reproduce, but verify reproducibility comes from the **pinned artifact**, not from re-running the GPU precompute (test on a second machine/GPU).
- [ ] **Approximation honesty:** results presented, but verify the per-link / white-interference / effective-SINR caveats are documented alongside any accuracy claim.

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Coordinate/units/scale mismatch (1) | MEDIUM | Add the empty-world Friis round-trip assert; fix the transform; regenerate the affected table; re-baseline fingerprints |
| Tx-power/convention mismatch (2) | MEDIUM | Pin down the link-budget contract string; fix the dB/linear or antenna-gain owner; re-run empty-world Friis check; regenerate table |
| SINR x-axis mismatch (3) | MEDIUM–HIGH | May require regenerating curves with the correct x-axis definition; re-validate at a known noise-limited point |
| Double-counting (4) | LOW–MEDIUM | Suppress the leaking statistical terms in the Sionna path; two-seed test confirms fix; re-baseline fingerprints |
| CQI/PHY table disagreement (5) | LOW | Re-point `getCqi()` at the Sionna table; ensure all-CQI storage; re-run self-consistency assert |
| Effective-SINR over-claim (6) | LOW | Add documentation + bounding spot-checks; no data regeneration needed unless the mapping itself is wrong |
| GPU/TF non-determinism (7) | LOW (if artifact pinned) / HIGH (if not) | Commit and pin the artifact; switch reproducible runs to load-only; create Sionna-specific baselines |
| Bad calibration tolerance (8) | LOW | Re-derive tolerance from physics; rewrite the calibration report with the explained residual |
| Precompute blow-up (9) | MEDIUM | Switch to batched single-run RT + effective-SINR method; drop interferer/RB axes for v1; re-estimate budget |
| Cache-invalidation bug (10) | MEDIUM | Fix the request-hash to cover/exclude the right inputs; embed request in artifact; add loud mismatch assert; invalidate stale tables |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls. (Phase names are suggested groupings for the roadmap, ordered by dependency.)

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 1. Coordinate/units/scale mismatch | Geometry & coordinate contract | Empty-world Friis round-trip within ~0.5–1 dB; node-overlay render |
| 2. Tx-power/convention mismatch | Geometry & coordinate contract / cache-format | Friis link budget reproduced within ~0.5–1 dB; no clean 10/20/30 dB offsets |
| 3. SINR x-axis mismatch | Link-to-system / BLER pipeline | Computed operating SNR equals curve anchor SNR at a noise-limited point |
| 4. Double-counting statistical path loss | SionnaChannelModel core (path-gain ownership) | Two-seed test → identical RSRP; only success-draw RNG consumed |
| 5. CQI/feedback vs. PHY table disagreement | CQI / feedback rewiring | Selected MCS has realized BLER ≤ target on the same table; all-CQI storage present |
| 6. Per-link/per-RB collapse, white interference, effective-SINR validity | Link-to-system / BLER pipeline (+ calibration spot-checks) | Approximation list documented; effective-SINR error bounded vs. Monte Carlo spot points |
| 7. GPU/TF non-determinism, fingerprints | Caching, artifact pinning & fingerprints | Second-machine/GPU reproducibility from pinned artifact; Sionna-specific baselines exist |
| 8. False equality expectation / no defensible tolerance | Empty-world / reference calibration | Tolerance is physics-derived; 3GPP residual quantified and explained |
| 9. Precompute cost blow-up | Precompute / offline-tool design | Size/time estimate done before full enumerator; batched RT + effective-SINR chosen |
| 10. Cache-invalidation correctness | Caching, artifact pinning & fingerprints | Each physical input change invalidates; non-physical inputs don't; loud mismatch abort |

**Phase ordering implication:** the **geometry/coordinate contract + empty-world calibration** phases must come **first** — they are the only thing standing between "plausible numbers" and "correct numbers," and Pitfalls 1, 2, 8 are validated there before any site-specific scene is trusted. **Path-gain ownership** (Pitfall 4) and the **BLER/SINR-x-axis pipeline** (3, 6) come next, then **CQI rewiring** (5), then **caching/pinning/fingerprints** (7, 10) which gate any committed fingerprinted scenario. **Precompute cost** (9) is a design gate that should be estimated early (during precompute/offline-tool design) even though it bites late.

## Sources

- Sionna RT 2.0.1 path-coefficient and channel-gain math (`a_n = (λ/4π)·C_R·(∏Tₙ)·C_T`, `g = Σ|aₙ|²` — antenna patterns and free-space factor baked in, Tx power not included): NVIDIA Sionna RT technical report and arXiv paper. HIGH confidence (official). https://arxiv.org/pdf/2504.21719 , https://arxiv.org/pdf/2303.11103
- Sionna RT antenna array / polarization (dual-/cross-polarized, configurable patterns): Sionna 2.0.1 docs. HIGH (official). https://nvlabs.github.io/sionna/rt/tutorials/Introduction.html
- TensorFlow GPU non-determinism (non-associative float reductions across thousands of threads; `TF_DETERMINISTIC_OPS` / `enable_op_determinism`, with residual non-deterministic ops): TensorFlow determinism RFC and issues. HIGH (official/maintainer). https://github.com/tensorflow/community/blob/master/rfcs/20210119-determinism.md , https://github.com/tensorflow/tensorflow/issues/2732
- Project design + verified Simu5G baseline behavior (analytic path loss + Gaussian shadowing + random LOS draw + scalar 38.901 penetration; `PhyPisaData::getBler` AWGN/TU `[3][15][49]` table; `getSINR()` interference summation; CQI inversion in `LteFeedbackComputationRealistic::getCqi()`): `Simu5G/.planning/PROJECT.md`, `Sionna/sionna-integration-plan.md` (§§3–7), `Sionna/sionna.elony.hatrany.md`. HIGH (verified in source per PROJECT.md).
- Link-to-system / effective-SINR mapping validity (EESM/MIESM compression valid only within calibrated range; white-interference assumption exact only for interference-free / non-IRC receivers): standard 3GPP link-to-system methodology (referenced in integration plan §5). MEDIUM (domain-standard, not re-verified against a specific paper here).

---
*Pitfalls research for: NVIDIA Sionna RT → Simu5G ray-tracing-to-system-level link abstraction (precompute-once, static scenarios)*
*Researched: 2026-06-17*
