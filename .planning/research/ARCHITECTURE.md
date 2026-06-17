# Architecture Research

**Domain:** Optional NVIDIA Sionna RT channel/PHY integration into Simu5G (OMNeT++/INET C++ system-level 5G NR simulator), precompute-once link-to-system, static scenarios
**Researched:** 2026-06-17
**Confidence:** HIGH (grounded in the actual Simu5G seam; file:line citations below)

## Standard Architecture

This is a brownfield extension that must slot into Simu5G's existing channel-model seam without
touching the default build. The integration spans two processes joined by one cached artifact:

```
┌───────────────────────────────────────────────────────────────────────────┐
│  OFFLINE (Python / TensorFlow / GPU) — never linked into the Simu5G binary  │
│  ┌──────────────────┐   reads    ┌───────────────────────────────────────┐  │
│  │ shared scenario  │──────────▶ │ sionna-precompute (CLI tool)          │  │
│  │ description (SSOT)│           │  - coord/param transform (OMNeT→Sionna)│  │
│  │ positions,antennas│           │  - RT trace all Tx/Rx pairs (1 run)   │  │
│  │ carrier,numerology│           │  - eff-SINR → reference curve → BLER   │  │
│  │ materials, MCS set│           │  - emit path-gain + BLER table         │  │
│  └──────────────────┘           └───────────────┬───────────────────────┘  │
│            │                      cache key = hash(full request)            │
└────────────┼──────────────────────────────────┼───────────────────────────┘
             │ (same file, asserted)             ▼
             │                        ┌──────────────────────┐
             │                        │ cached artifact      │
             │                        │ (HDF5/JSON, pinned)  │  schema-versioned
             │                        │ table + manifest     │
             │                        └──────────┬───────────┘
┌────────────▼───────────────────────────────────▼──────────────────────────┐
│  ONLINE (C++ / OMNeT++ / INET) — default build unchanged                    │
│  ┌──────────────────────┐  loads once  ┌──────────────────────────────┐    │
│  │ SionnaManager        │◀────────────▶│ in-memory SionnaTable        │    │
│  │ (global, 1 per net)  │   owns       │ (link,MCS)[,SINRbin v2] →     │    │
│  │  - loads artifact    │              │   BLER ; (link)→ pathGain     │    │
│  │  - asserts manifest  │              └──────────────┬───────────────┘    │
│  │    vs scenario SSOT  │                             │ lookup (read-only) │
│  └──────────────────────┘            ┌────────────────┼──────────────────┐ │
│        ▲ (ModuleRef)                 ▼                ▼                  │ │
│  ┌─────┴────────────────┐   ┌──────────────────┐  ┌────────────────────┐│ │
│  │ SionnaChannelModel   │   │ SionnaFeedback   │  │ (per carrier, per  ││ │
│  │ : LteRealisticChannel│   │ Computation      │  │  PHY instance)     ││ │
│  │  override path-gain  │   │ getCqi() → same  │  │ thin lookups only  ││ │
│  │  override BLER lookup │   │ table            │  └────────────────────┘│ │
│  └──────────────────────┘   └──────────────────┘                        │ │
│   selected via NED `like ILteChannelModel` + channelModelType ini string  │ │
└───────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility (owns) | Implementation anchor |
|-----------|----------------------|-----------------------|
| **Shared scenario description (SSOT)** | The single source for positions, antenna heights/orientations, carrier freq, numerology/SCS, band count (`numBands_`), materials, MCS/CQI set. Feeds *both* processes. | New file (YAML/JSON) checked into the scenario; both the Python tool and the C++ side read it (or the C++ side reads the ini that the tool also consumed). |
| **`sionna-precompute` (offline CLI)** | Owns the RT physics and the SINR→BLER *function*. Reads SSOT, applies the coord/param transform, traces all Tx/Rx pairs in one Sionna run, computes effective-SINR→reference-curve BLER per (link, MCS), emits the table + a manifest, writes/reads the cache by request hash. | Standalone Python entry point in `Sionna/` venv. No coupling to the C++ build. |
| **Cached artifact (table + manifest)** | The only cross-process contract. Holds per-link path gains and per-(link,MCS) BLER (v1); per-(link,MCS,SINR-bin) BLER + path gains (v2). Manifest carries the parameter contract for fail-loud assertion. Schema-versioned and pinned for fingerprints. | HDF5 (recommended) or JSON; co-located with scenario. |
| **`SionnaManager` (global owner)** | One per network. Loads the artifact once at the init stage where positions are final, parses it into the in-memory `SionnaTable`, and **asserts the manifest matches the live scenario** (carrier/numerology/band count/positions) — fails loudly on mismatch. Provides read-only lookup API. | New `cSimpleModule`, one instance in the network NED (sibling to `binder`). Referenced via `ModuleRefByPar`. |
| **`SionnaTable` (in-memory)** | The lookup data structure: `(linkId, MCS) → BLER`, `linkId → pathGain` (v1); add a SINR-bin axis (v2). Owned by `SionnaManager`. | Plain C++ struct/maps inside `SionnaManager`. |
| **`SionnaChannelModel : LteRealisticChannelModel`** | Per-PHY, per-carrier thin lookup. Overrides the **path-gain** source (so RSRP/desired/interferer powers are site-specific) and the **BLER lookup** (replaces `PhyPisaData::getBler`). *Keeps* the inherited `getSINR()` interference+noise aggregation and the success-draw/HARQ heuristic. | New class in `src/simu5g/stack/phy/channelmodel/`, selected via `like ILteChannelModel`. |
| **`SionnaFeedbackComputation`** | CQI selection from the *same* SionnaTable (highest MCS with BLER ≤ target), so scheduler MCS and realized BLER agree. | New `LteFeedbackComputation` subclass paralleling `LteFeedbackComputationRealistic`; reads `SionnaManager`'s table, not `binder->phyPisaData`. |
| **Coord/param transform** | The OMNeT++↔Sionna coordinate (origin, axes, units, scale) and parameter (Tx-power convention, polarization, SINR x-axis definition) mapping. | **Owned by the offline tool** (it is the side that meets Sionna's geometry); the C++ side only asserts the resulting manifest. See ownership rationale below. |

## Where this hooks into the real Simu5G seam (verified)

| Existing seam | File:line | What Sionna overrides |
|---------------|-----------|-----------------------|
| Polymorphic selection | `LteNicBase.ned:101` (`channelModel[numCarriers]: <lteChannelModelType> like ILteChannelModel`), `NrNicUe.ned:71` (`nrChannelModel[...]`), default at `LteNicBase.ned:48` / `NrNicUe.ned:33` | Set `lteChannelModelType`/`nrChannelModelType` ini string to `"SionnaChannelModel"`. **No NED edit needed** — selection is already a string param. This is the zero-impact opt-in. |
| PHY → channel binding | `LtePhyBase.ned:44` (`channelModelModule`), `LtePhyBase.cc:148-169` `initializeChannelModel()` | Unchanged; `SionnaChannelModel` is a drop-in `LteChannelModel`. |
| BLER lookup (reception) | `LteRealisticChannelModel.cc:1796` `binder_->phyPisaData.getBler(itxmode, cqi, snr)` (and `:1947` for D2D) | Replace the lookup expression with a `SionnaManager` table lookup keyed by link+MCS (v1) / link+MCS+SINR (v2). Keep the surrounding RB loop, `uniform(0,1)≤BLER` draw (`:1819`) and `harqReduction_` (`:1817`). |
| Path gain / RSRP | `getAttenuation()` / `computePathLoss()` (declared `LteChannelModel.h:94,101`); consumed in `getSINR`/`getRSRP` recvPower assembly `LteRealisticChannelModel.cc:~509-518` | Override `getAttenuation()`/`computePathLoss()` to return the Sionna per-link path gain instead of the 3GPP formula + shadowing + LOS draw. This is the no-double-counting boundary: when Sionna is active, statistical shadowing/LOS/penetration are not applied. |
| CQI feedback | `LteFeedbackComputationRealistic.cc:24` ctor takes `&(binder->phyPisaData)`; `getCqi()` at `:83-106` inverts the BLER table | Provide `SionnaFeedbackComputation` whose `getCqi()` inverts the **SionnaTable**. Both reception and feedback then read one table — the consistency requirement. |
| Shared table today | `Binder.h:448` `PhyPisaData phyPisaData;` (public) — read by both reception (`binder_->phyPisaData`) and feedback ctor | This is the precedent: the table is already a single Binder-owned object read by both paths. Sionna mirrors this pattern but in `SionnaManager` (see Binder-vs-SionnaManager). |

**Key structural insight:** `NrChannelModel extends LteRealisticChannelModel` (NrChannelModel.h:20). Sionna
should follow the same inheritance — derive from `LteRealisticChannelModel` (not the abstract
`LteChannelModel`) so it inherits the entire `getSINR()` interference+noise machinery for free and
overrides only the two hooks (path gain in, BLER out). This is exactly the "Simu5G owns SINR value,
Sionna owns SINR→BLER" division of labor.

## Architectural Patterns

### Pattern 1: Global precompute owner, thin per-PHY lookup

**What:** One network-global module loads the artifact once; the many per-carrier/per-PHY
`SionnaChannelModel` instances hold no data, only a `ModuleRefByPar<SionnaManager>` and do read-only
lookups.
**When to use:** Always here — tracing all Tx/Rx pairs in one Sionna run is far cheaper than N link
calls, and the table is identical across all PHY instances.
**Trade-offs:** Slightly more wiring (one extra module + reference) vs. a giant memory win and a single
load/assert point. This mirrors the existing `Binder::phyPisaData` single-table pattern.

### Pattern 2: Manifest-asserted contract at init (fail loud)

**What:** The artifact carries a manifest (carrier, numerology, band count, positions hash, antenna
config, Tx-power convention, SINR x-axis definition). `SionnaManager` asserts it against the live
scenario at the init stage where positions are final; any mismatch throws `cRuntimeError`.
**When to use:** Mandatory — a silent geometry/unit mismatch "looks plausible while being wrong"
(PROJECT.md constraint). The reference/empty-world calibration mode exists precisely to exercise this.
**Trade-offs:** Requires disciplined schema versioning; pays for itself by catching the worst failure class.

### Pattern 3: One table, two readers (reception + feedback)

**What:** Both `isReceptionSuccessful()` and `getCqi()` read the *same* SionnaTable. The table must
therefore hold BLER for **all** CQIs/MCS per link, not just the selected one.
**When to use:** Always — otherwise scheduler MCS and realized BLER disagree (PROJECT.md requirement).
This generalizes the existing `binder->phyPisaData` shared-table arrangement.

### Pattern 4: v1-as-strict-subset extension point (the extra table dimension)

**What:** v1 keys BLER by `(linkId, MCS)` (noise-limited: SINR=SNR is static → one BLER per link/MCS).
v2 adds a SINR-bin axis: `(linkId, MCS, SINR-bin) → BLER`, and the artifact additionally returns path
gains so Simu5G's existing interference summation places the operating point on the curve.
**When to use:** Design the table API and schema with the SINR axis present-but-degenerate in v1 (single
bin / "infinite SINR" sentinel) so v2 is an additive change, not a refactor.
**Trade-offs:** A tiny amount of v1 over-design (an axis of size 1) buys a non-breaking v2. The C++ lookup
signature `lookupBler(linkId, mcs, sinr)` should exist from v1, ignoring `sinr` in the noise-limited path.

## Data Flow

### Build/precompute flow (offline → artifact)

```
shared scenario description (positions, antennas, carrier, numerology, materials, MCS set)
    ↓ read by tool
sionna-precompute: coord/param transform → RT trace all Tx/Rx pairs (one run)
    ↓
effective-SINR → reference-curve → BLER per (link, MCS)   +   per-link path gains
    ↓ hash(full request) = cache key
cached artifact (table + manifest), pinned/committed
```

### Runtime lookup flow (artifact → packet decision)

```
init (positions final)
    ↓
SionnaManager.load(artifact) → assert manifest vs scenario SSOT (fail loud)
    ↓ in-memory SionnaTable
─ reception path ─────────────────────────────────────────────
LteAirFrame → SionnaChannelModel::getSINR()  [inherited interference+noise aggregation,
                                               but desired/interferer power from Sionna path gain]
    ↓ SINR
SionnaChannelModel::isReceptionSuccessful() → SionnaTable.lookupBler(link, MCS[, SINR])
    ↓ × harqReduction_^(txNum-1) ; uniform(0,1) ≤ BLER
success / failure → up the stack
─ feedback path ──────────────────────────────────────────────
SionnaFeedbackComputation::getCqi() → SionnaTable (same) → highest MCS with BLER ≤ targetBler
    ↓
CQI report → scheduler  (MCS now agrees with realized BLER)
```

**Direction summary:** SSOT → tool → artifact is write-once, offline. Artifact → SionnaManager →
table is load-once at init. Table → channel model / feedback is read-only at runtime. Positions flow
*from* OMNeT++ into the SSOT/tool (the tool is the consumer of geometry); the C++ side only re-asserts
that the loaded table was built for these positions.

## Coordinate / parameter-contract ownership

**Recommendation: the offline `sionna-precompute` tool owns the OMNeT++↔Sionna transform; the C++
side owns only assertion.**

Rationale: the transform (origin, axes, units, scale, antenna height/orientation, Tx-power convention,
polarization, SINR x-axis definition) is fundamentally a property of how the *Sionna scene* is built,
and only the Python side touches Sionna geometry and materials. Putting the transform in C++ would mean
the binary must understand Mitsuba scene conventions — exactly the dependency the project forbids. The
C++ side's job is the cheap, robust half: read the manifest, compare to the live scenario, and throw on
mismatch. This keeps the heavy, Python-only knowledge offline and the safety check online.

## Precompute owner: dedicated `SionnaManager` (recommended) vs. extending `Binder`

**Recommendation: a dedicated `SionnaManager` module, not `Binder`.**

| Criterion | Extend `Binder` | Dedicated `SionnaManager` |
|-----------|-----------------|---------------------------|
| Default-build impact | Binder is core, always present; adding Sionna load/assert code (even guarded) bloats the hot path and risks touching default behavior/fingerprints | Module only instantiated when Sionna is opt-in; zero presence in default builds → satisfies the "byte-for-byte unaffected" constraint cleanly |
| Precedent | `Binder` already owns the analogous shared table (`phyPisaData`, Binder.h:448) read by both reception and feedback — so Binder *could* host it | Mirrors that single-table pattern in an isolated module; same access shape via `ModuleRefByPar` |
| Separation of concerns | Binder is the global registry (cells, carriers, IDs); RT-table loading is unrelated | Sionna-specific lifecycle (load, manifest-assert, schema version) stays self-contained |
| Optional dependency hygiene | Risks pulling Sionna concepts into a universally-compiled class | Confines Sionna code to files only compiled/instantiated for Sionna runs |

The decisive factor is the project's hard constraint that the default build and behavior stay
unaffected. A dedicated module that simply does not exist in a default network keeps that guarantee
trivially, while reusing Binder's *pattern* (one global table, two readers). `SionnaManager` is referenced
by `SionnaChannelModel` and `SionnaFeedbackComputation` via `ModuleRefByPar`, exactly as channel models
already reference `Binder`.

## Recommended Project Structure

```
Sionna/                                  # offline, Python venv (existing)
├── sionna_precompute/                   # NEW: the offline tool (package)
│   ├── __main__.py                      # CLI entry: read SSOT → trace → emit artifact
│   ├── scenario.py                      # SSOT loader + manifest builder
│   ├── transform.py                     # OMNeT++↔Sionna coord/param transform (OWNER)
│   ├── rt.py                            # RT trace all Tx/Rx pairs, one run
│   ├── link_to_system.py               # eff-SINR → reference curve → BLER
│   ├── cache.py                         # hash(full request) keying, read/skip
│   └── schema.py                        # artifact schema + version
└── scenarios/<name>/scenario.yaml       # shared scenario description (SSOT)
                       /artifact.h5       # cached, pinned table + manifest

Simu5G/src/simu5g/                       # online, C++ (default build untouched)
├── stack/phy/channelmodel/
│   ├── SionnaChannelModel.{h,cc,ned}    # NEW: : LteRealisticChannelModel, like ILteChannelModel
│   └── (LteRealisticChannelModel, NrChannelModel unchanged)
├── stack/phy/feedback/
│   └── SionnaFeedbackComputation.{h,cc} # NEW: getCqi() over SionnaTable
└── sionna/                              # NEW subdir to isolate Sionna-only code
    ├── SionnaManager.{h,cc,ned}         # NEW: global owner, load + assert + table
    └── SionnaTable.{h,cc}               # NEW: in-memory lookup structure + loader
```

### Structure Rationale

- **`Sionna/sionna_precompute/`:** all Python/TF/GPU code stays in the existing venv, never linked
  into the C++ binary. `transform.py` is the explicit owner of the coordinate/parameter contract.
- **New `src/simu5g/sionna/` subdir:** physically isolates Sionna-only C++ so it is obviously
  separable and never compiled into a default-only configuration if the build is later partitioned.
- **`SionnaChannelModel` beside the existing channel models:** it is selected by the same NED
  `like ILteChannelModel` mechanism, so co-location is natural and requires no NED interface change.

## Suggested Build Order (with dependencies)

The order is driven by: contract first, then offline producer, then online consumer, then the
consistency wiring, then validation. Each step is independently testable.

1. **Shared scenario description + manifest schema (the contract).** *Depends on:* nothing. Define the
   SSOT fields and the artifact/manifest schema (incl. the v1-degenerate SINR axis). Everything else
   references this. *Gate:* schema versioned, fields cover the parameter contract.
2. **Coord/param transform + RT trace in the offline tool.** *Depends on:* (1). Implement `transform.py`
   and `rt.py`; produce per-link path gains for the empty-world reference scene first. *Gate:* path
   gains for the reference world are sane vs. free-space.
3. **Link-to-system BLER + cache + artifact writer.** *Depends on:* (2). Effective-SINR→reference-curve
   BLER per (link, MCS); hash-keyed cache; emit artifact + manifest. *Gate:* artifact round-trips;
   rerun with same request hits cache and skips Sionna.
4. **`SionnaManager` + `SionnaTable` (load + assert).** *Depends on:* (1) for schema, (3) for a real
   artifact. Network module that loads, parses, and fail-loud asserts the manifest. *Gate:* loads the
   reference artifact; mismatched manifest throws.
5. **`SionnaChannelModel` reception path.** *Depends on:* (4). Derive from `LteRealisticChannelModel`;
   override path gain (`getAttenuation`/`computePathLoss`) and the BLER lookup at the
   `getBler` call site; keep `getSINR` aggregation + HARQ/success draw. Select via the
   `channelModelType` ini string. *Gate:* a run with Sionna selected produces packets and SINR using
   table path gains; default build/behavior unchanged (regression-test the default).
6. **`SionnaFeedbackComputation` (CQI from same table).** *Depends on:* (4) and (5). `getCqi()` inverts
   the SionnaTable. *Gate:* scheduler MCS and realized BLER agree on a controlled link.
7. **Empty-world calibration + fingerprint baselines.** *Depends on:* (5),(6). Run the reference world;
   confirm bounded, explainable difference vs. the 3GPP statistical model; pin the artifact and create
   Sionna-specific fingerprint baselines (RNG-stream consumption differs). *Gate:* fingerprints stable
   across reruns from the pinned table.
8. **"Some world" authored scene** (synthetic boxes or built-in Munich) to demonstrate the
   site-specific capability. *Depends on:* the whole v1 pipeline (1–7).

**v1→v2 extension (later, additive):** extend the artifact with a SINR-bin axis + curves, fill the
already-present `sinr` parameter in `SionnaTable.lookupBler(link, mcs, sinr)`, and let the inherited
`getSINR()` interference summation move the operating point. No structural change to components 4–6.

## Anti-Patterns

### Anti-Pattern 1: Embedding Python/RT in the C++ runtime or per-TTI coupling

**What people do:** Spawn Sionna or call into Python during the simulation loop.
**Why it's wrong:** Reintroduces the Python/TF/GPU dependency into runs, destroys determinism, and
violates the precompute-once scope.
**Do this instead:** Strict offline tool → cached artifact → load-once. C++ never imports Python.

### Anti-Pattern 2: Double-counting path effects

**What people do:** Apply Sionna path gain *and* leave Simu5G's statistical shadowing / random LOS /
penetration loss active.
**Why it's wrong:** Deterministic RT gain plus random statistics is physically meaningless.
**Do this instead:** When `SionnaChannelModel` is active it fully owns path gain; override
`getAttenuation`/`computePathLoss` to bypass the statistical layers entirely.

### Anti-Pattern 3: Two tables (reception vs. feedback drift)

**What people do:** Let `getCqi()` keep reading `phyPisaData` while reception reads the Sionna table.
**Why it's wrong:** Scheduler picks an MCS calibrated to AWGN/TU curves while reception uses
site-specific BLER → systematic mismatch.
**Do this instead:** One `SionnaTable`, both readers; store BLER for all CQIs/MCS per link.

### Anti-Pattern 4: Hard-coding the noise-limited assumption into the schema

**What people do:** Make the table key `(link, MCS)` with no room for an SINR axis.
**Why it's wrong:** v2 then requires a schema + lookup-signature refactor.
**Do this instead:** Ship `lookupBler(link, mcs, sinr)` and a degenerate SINR axis in v1.

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| NVIDIA Sionna RT (Python/TF/GPU) | Offline subprocess/CLI producing a cached artifact; never linked into C++ | Lives in sibling venv; GPU/TF float nondeterminism is absorbed by pinning the artifact |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| offline tool ↔ C++ | Cached artifact file (HDF5/JSON) + manifest | The only cross-process contract; schema-versioned, hash-keyed, pinned |
| `SionnaManager` ↔ `SionnaChannelModel`/`SionnaFeedbackComputation` | `ModuleRefByPar<SionnaManager>`, read-only table lookup | Mirrors how channel models reference `Binder` |
| `SionnaChannelModel` ↔ `LtePhyBase` | Existing `like ILteChannelModel` / `channelModelModule` seam | No NED interface change; selection is a string param |
| scenario SSOT ↔ both processes | File read on each side + manifest assertion in C++ | Fail-loud on mismatch is mandatory |

## Sources

- Simu5G source (verified, this repo): `LteChannelModel.h:85-156`, `ILteChannelModel.ned:22`,
  `PhyPisaData.h:44` (`getBler`), `LtePhyBase.ned:44`, `LtePhyBase.cc:148-169`,
  `LteRealisticChannelModel.h:46`/`.cc:1709-1838` (`isReceptionSuccessful`, `getBler@1796`,
  `harqReduction@1817`, success draw@1819), `LteRealisticChannelModel.cc:~509-518` (recvPower/path-gain
  assembly), `LteFeedbackComputationRealistic.cc:24` (ctor), `:83-106` (`getCqi`),
  `LteNicBase.ned:48,101`, `NrNicUe.ned:33,71`, `NrChannelModel.h:20` (extends pattern),
  `Binder.h:448` (`phyPisaData` shared table) — Confidence HIGH.
- Design source of truth: `Sionna/sionna-integration-plan.md` §3 (division of labor), §4
  (architecture/seams/invocation), §6 (geometry/transform); `Simu5G/.planning/PROJECT.md` (decisions,
  constraints) — Confidence HIGH.

---
*Architecture research for: optional Sionna RT integration into Simu5G (static, precompute-once link-to-system)*
*Researched: 2026-06-17*
