<!-- GSD:project-start source:PROJECT.md -->

## Project

**Simu5G × Sionna Integration**

An **optional, opt-in** channel-model module that lets Simu5G replace its statistical
PHY/link abstraction with results derived from **NVIDIA Sionna RT** (ray tracing over real
3D geometry), giving site-specific, physically grounded propagation and BLER. It is built
as a reusable Simu5G capability for the research community: the default Simu5G build and
behavior stay completely unaffected, and no Python/TensorFlow/GPU dependency is added to a
normal build or run. Initial scope targets **completely static scenarios** with
precompute-once channel data.

**Core Value:** Simu5G can opt in to site-specific, geometry-derived channel/BLER from Sionna **without
changing the default build or behavior** — the analytic model remains the untouched default,
and Sionna is a clean, selectable alternative.

### Constraints

- **Tech stack**: Simu5G / OMNeT++ / INET, C++. Sionna RT is Python/TensorFlow/GPU — must stay
  outside the normal build; coupling is via a precomputed, cached data artifact.

- **Compatibility**: default Simu5G build, behavior, and existing channel models must remain
  byte-for-byte unaffected; Sionna is strictly opt-in.

- **Determinism / fingerprints**: runs must be reproducible from the pinned cached table;
  Sionna configs need their own fingerprint baselines.

- **Parameter contract**: carrier frequency, bandwidth, numerology/SCS, band count, antenna
  arrays/patterns, Tx-power convention, polarization, and SINR x-axis definition must be
  identical on both sides; mismatch must fail loudly at init.

- **Validation honesty**: exact numerical agreement between Sionna (empty world) and Simu5G's
  3GPP statistical formulas is not expected; the target is a bounded, explainable difference.
<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->

## Technology Stack

## Headline findings (read these first)

## Recommended Stack

### Core Technologies (offline tool venv only)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **Python** | 3.13.7 (≥3.11 required by sionna.phy/sys; ≥3.10 by sionna-rt) | Host language for the offline tool | Already the venv interpreter; matches installed wheels. |
| **sionna-rt** | **2.0.1** (latest; released 2026-03-31) | Differentiable ray tracing: scene load, path solving, CIR/CFR, radio maps | The stand-alone RT package. Verified API surface below. Built on Mitsuba 3 + Dr.Jit; **TF-free**. |
| **sionna** (phy+sys) | **2.0.1** | `sionna.phy` (5G NR LDPC, MCS decode, transport-block math) + `sionna.sys.PHYAbstraction` (EESM + shipped BLER tables) | Provides the **entire** effective-SINR→BLER step as a supported, calibrated library. PyTorch-backed, no TF. |
| **Mitsuba** | **3.8.0** | Scene representation (XML/`.ply` meshes), EM-material backend, the `*_ad_mono_polarized` variant Sionna RT drives | Sionna RT's geometry/render engine; the scene file format. Auto-selected variant — do not override. |
| **Dr.Jit** | **1.3.1** | JIT/autodiff array backend under Mitsuba/Sionna RT; LLVM CPU or CUDA GPU | Transparent; pulled by sionna-rt. LLVM needed for CPU runs. |
| **PyTorch** | **2.12.0** | Tensor backend for sionna.phy/sionna.sys | Replaces TensorFlow in 2.x. The only "heavy ML" dep, and it stays in the offline venv. |
| **NumPy** | 2.4.6 | Array glue, `out_type="numpy"` extraction from RT, writing HDF5 | Universal; `Paths.cfr(..., out_type="numpy")` hands back plain arrays for the C++-bound table. |
| **h5py** | **3.16.0** | Write/read the cached `(link, MCS)` BLER + path-gain artifact | Already installed; self-describing, typed, attribute-rich. See §Artifact schema. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **SciPy** | 1.17.1 | Interpolation / curve fitting if a custom effective-SINR or BLER smoothing is ever needed | Only if customizing beyond `PHYAbstraction` defaults. |
| **Matplotlib** | (in venv via rt scripts) | Render/inspect scenes, sanity-plot BLER-vs-SINR during calibration | Dev/validation only; never a build dep. |
| **LLVM** | system | Dr.Jit CPU backend | Required to run RT on CPU (no GPU). GPU path uses CUDA via Dr.Jit instead. |
| **HighFive** *(C++)* or hand-rolled C++ HDF5 reader | latest | C++-side reader for the artifact, if HDF5 chosen | Only the **reader**; header-only HighFive avoids pulling the full HDF5 C++ API into Simu5G. Alternatively read raw HDF5 C lib, or pick the binary+JSON variant (§variants) to avoid an HDF5 C++ dep entirely. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| venv at `/home/zoli/Projects/OMNET/Sionna/venv` | Isolated offline-tool environment | Already provisioned with the exact versions above. Pin them in a `requirements.txt`/lockfile for reproducibility. |
| Mitsuba/Blender + (optional) OpenStreetMap → Mitsuba export | Authoring "some world" scenes | Sionna ships `sionna.rt.scene.munich`, `etoile`, etc. for the demo scene; Blender + the Mitsuba-Blender add-on (or `sionna.rt` OSM tooling) for custom maps. |
| Request-hash manifest (JSON) | Cache key + parameter contract | One small JSON sidecar per artifact; hash of scene+positions+materials+antennas+freqs+powers+MCS set. |

## Verified Sionna RT 2.x API surface (against installed 2.0.1)

- **Sub-carrier frequencies:** `subcarrier_frequencies(num_subcarriers, subcarrier_spacing) -> Float`.
- **Channel frequency response (the key call for per-RB SINR):**
- **CIR (if path-level data wanted for the schema / future Doppler):** `Paths.cir(sampling_frequency=1.0, num_time_steps=1, normalize_delays=True, out_type=...)`.
- Constructor loads + interpolates **shipped BLER tables** (`load_bler_tables_from='default'`) on a fine SINR×code-block-size grid per MCS; defaults: `MCSDecoderNR`, `TransportBlockNR`, EESM via `sinr_effective_fun`. Swap to **MIESM** by passing a different `sinr_effective_fun`.
- Pipeline: pass **per-subcarrier (post-equalization) SINR** + `mcs_index` (+ `mcs_table_index` for the 5G-NR MCS table) → get **BLER/TBLER** directly. This *is* the design plan's "RT → post-eq effective SINR → BLER via reference curves" — already implemented and calibrated.

## Chosen effective-SINR → BLER approach

- **Why EESM/PHYAbstraction over hand-built curves:** EESM/MIESM are the standard, literature-validated link-to-system mappings for OFDM/5G-NR; `PHYAbstraction` ships calibrated reference tables and the MCS/transport-block machinery (38.211/212/214), so we get curves "for free," consistent with NR, without running our own LDPC Monte Carlo.
- **Why not full link-level LDPC Monte Carlo in v1:** higher fidelity but far costlier per (link, MCS) entry; deferred per PROJECT.md Out-of-Scope. `PHYAbstraction` is the cheap, fast-iteration path. (If higher fidelity is later wanted, `sionna.phy.nr` PUSCH/PDSCH chains exist in the same venv — clean upgrade, no new deps.)
- **x-axis contract:** `PHYAbstraction` consumes **post-equalization SINR**; the design plan §5 contract ("Simu5G's signal/(interference+noise) maps onto the curve's average-SNR x-axis") holds **iff** v1 builds curves under the same convention. v1 is noise-limited, so SINR=SNR and the mapping is direct. **Pin EESM-vs-MIESM and the `mcs_table_index` once, identically, on both sides.**
- **MIESM option:** more accurate for high-order QAM; available by passing `sinr_effective_fun=MIESM(...)`. Start with default EESM; switch if empty-world calibration shows bias.

## Chosen artifact / exchange format

### Schema sketch (v1, noise-limited: one BLER per (link, MCS), table holds all MCS/CQI per link)

- **Determinism/fingerprints:** commit/pin the `.h5`; record `request_hash` + library versions in attrs so reruns verify the cache and Sionna fingerprints stay reproducible regardless of GPU/float nondeterminism.
- **C++ reader:** prefer header-only **HighFive** to read HDF5 without dragging the heavy HDF5 C++ API into Simu5G; fall back to the raw HDF5 C lib. If adding *any* HDF5 to the Simu5G repo is unwanted, use the **binary+JSON variant** below.

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `sionna.sys.PHYAbstraction` (EESM + shipped tables) | Full `sionna.phy.nr` PUSCH/PDSCH LDPC Monte Carlo | When v1's link-to-system fidelity is insufficient; same venv, no new deps — clean upgrade path. |
| EESM (PHYAbstraction default) | MIESM (`sinr_effective_fun=MIESM`) | When empty-world calibration shows bias at high-order QAM / high code rate. |
| HDF5 (`h5py`) artifact | NumPy `.npz` | Simpler if you accept weaker self-description and a NumPy-format C++ reader; HDF5's attributes + typing better fit the parameter contract. |
| `Paths.cfr` per-subcarrier H | `Paths.taps` (tapped-delay-line) | If a time-domain TDL is preferred for the effective-SINR computation; `cfr` is the direct frequency-domain route for per-RB SINR. |
| `PathSolver` per Tx/Rx batch | `RadioMapSolver` (`RadioMap.path_gain`) | Use radio maps for wide-area path-gain/coverage and as the empty-world calibration cross-check; use `PathSolver` for the precise per-link CIR/CFR feeding BLER. |
| GPU (CUDA via Dr.Jit) | CPU (LLVM via Dr.Jit) | CPU when no GPU is available; offline precompute tolerates slower CPU RT. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **TensorFlow** / any TF-era Sionna 0.x–1.x API (`sionna.channel`, TF tensors, `sn.rt` TF objects) | Sionna 2.x is **Dr.Jit/Mitsuba (RT) + PyTorch (PHY/SYS)**; TF is *not installed* and the old API does not exist. Training-data examples will not run. | Sionna RT 2.0.1 + `sionna.phy`/`sionna.sys` (PyTorch) as verified above. |
| **Embedding Python/TF/PyTorch/GPU in the Simu5G build or per-TTI runtime calls** | Violates the hard constraint (no Python/TF/GPU in a normal build); kills determinism and speed; pybind11/GIL/kernel-launch overhead. | Offline precompute → cached HDF5 artifact; C++ side only reads one file. |
| **Hand-rolling EESM/MIESM or sourcing external BLER curves** | `PHYAbstraction` already implements EESM and ships calibrated 5G-NR LDPC tables with correct MCS/transport-block handling; rolling your own risks silent x-axis/MCS-table mismatch. | `sionna.sys.PHYAbstraction(mcs_index, sinr=...)`. |
| **JSON for the bulk numeric BLER/path-gain table** | Text-bloated, lossy on floats, slow to parse, no typing/attributes; awful for `[L,M]`/`[L,M,S]` arrays. | HDF5 for arrays; JSON only for the small request/manifest sidecar. |
| **Opaque hand-packed binary blob as the primary artifact** | Not self-describing; brittle across schema/endianness/version; defeats the "fail loud on parameter mismatch" requirement. | HDF5 with typed datasets + contract attributes (or, if no HDF5 dep allowed in C++, a *versioned, documented* little-endian binary table **paired with the JSON manifest** carrying the contract + hash). |
| **`synthetic_array=True` when antenna geometry matters** | Approximates the array as a single point with phase shifts; fine for speed but can misrepresent true per-element channels. | Set `synthetic_array=False` (as `rt_step2.py` does) when array realism matters; keep `True` only as a speed knob you've validated. |
| **Overriding the Mitsuba variant** | Sionna RT auto-selects the required `*_ad_mono_polarized` variant; changing it breaks differentiability/polarization. | Leave `mi.variant()` as auto-selected (the rt scripts already note this). |
| **`sionna-no-rt` / `sionna-vispy` for this task** | `no-rt` drops the RT engine you need; `vispy` is a visualization backend, not required. | `sionna-rt` + `sionna` (full). |

## Stack Patterns by Variant

- Run Dr.Jit on the CUDA backend; batch all Tx/Rx pairs in one `PathSolver`/`RadioMapSolver` call per scene (design plan §4.1 global precompute).
- Because GPU/TF-float results can be nondeterministic, **always pin the produced HDF5** as the reproducible source of truth.
- Dr.Jit uses the **LLVM** backend (must be present). RT is slower but the precompute-once model tolerates it. Reduce cost with `merge_shapes=True` and a modest `max_depth` (3–5).
- Emit a **versioned little-endian binary** dataset table + a **JSON manifest** (contract attrs + `request_hash`). C++ reads the manifest (assert/fail-loud), then mmaps/reads the binary `[L,M]` array. Keep HDF5 as the canonical/debug format in the offline tool.
- Switch the table from scalar `/bler[L,M]` to `/bler_curve[L,M,S]` over `/sinr_grid[S]`; Simu5G keeps owning the SINR value and looks up the curve. v1's scalar is the `S`-fixed slice — additive, nothing discarded.

## Version Compatibility

| Package | Version (installed/verified) | Notes |
|---------|------------------------------|-------|
| python | 3.13.7 | sionna.phy/sys need ≥3.11; sionna-rt needs ≥3.10. venv is 3.13. |
| sionna-rt | 2.0.1 | latest; 2026-03-31. Built on Mitsuba 3 + Dr.Jit. |
| sionna | 2.0.1 | phy+sys (PyTorch). `import sionna.rt/phy/sys` all OK in venv. |
| mitsuba | 3.8.0 | Sionna RT 2.0.1's geometry/render backend; scene format. |
| drjit | 1.3.1 | array/JIT/autodiff backend; LLVM (CPU) or CUDA (GPU). |
| torch | 2.12.0 | backend for sionna.phy/sys. **No TensorFlow present.** |
| numpy | 2.4.6 | array glue; `out_type="numpy"` extraction. |
| h5py | 3.16.0 | artifact writer/reader. |
| scipy | 1.17.1 | optional interpolation/curve work. |

## Sources

- [sionna-rt · PyPI](https://pypi.org/project/sionna-rt/) (2.0.1, 2026-03-31; Mitsuba 3 / Dr.Jit / LLVM; TF not required)
- [Sionna Documentation — 2.0.1](https://nvlabs.github.io/sionna/) and [Installation — 2.0.1](https://nvlabs.github.io/sionna/installation.html)
- [Announcing Sionna 1.0 · NVlabs/sionna Discussion #776](https://github.com/NVlabs/sionna/discussions/776) (module split RT/PHY/SYS; SYS computes BLER from post-equalization SINR)
- [Sionna RT: Technical Report (arXiv 2504.21719)](https://arxiv.org/pdf/2504.21719) (Dr.Jit/Mitsuba rewrite, differentiability)
- [System-Level Simulations — Sionna SYS tutorial](https://nvlabs.github.io/sionna/sys/tutorials/End-to-End_Example.html) and SYS PHY-abstraction API (EffectiveSINR / EESM / PHYAbstraction)
- [New Radio Physical Layer Abstraction for System-Level Simulations of 5G Networks (arXiv 2001.10309)](https://arxiv.org/pdf/2001.10309) (EESM/MIESM link-to-system methodology, NR LDPC)
- [An optimal calibration factor for MIESM in 5G NR — Electronics Letters 2024](https://ietresearch.onlinelibrary.wiley.com/doi/full/10.1049/ell2.13159) (MIESM calibration)
- **Installed-package introspection** (`/home/zoli/Projects/OMNET/Sionna/venv`): verified versions and the exact signatures of `PathSolver.__call__`, `Paths.cfr/cir/taps/a/tau`, `subcarrier_frequencies`, `RadioMapSolver.__call__`, `RadioMap.path_gain/rss/sinr`, `PHYAbstraction.__init__/call` — HIGH confidence, primary source.
- Local context: `Sionna/sionna-integration-plan.md`, `Sionna/sionna.elony.hatrany.md`, `Sionna/rt_step1.py`, `Sionna/rt_step2.py`, `Simu5G/.planning/PROJECT.md`.

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
