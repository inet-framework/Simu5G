# Stack Research

**Domain:** Offline ray-tracing channel/PHY-abstraction tool bridging Sionna RT (Python/GPU) → cached artifact → Simu5G (C++/OMNeT++), static scenarios, link-to-system methodology.
**Researched:** 2026-06-17
**Confidence:** HIGH (all Sionna RT/PHY/SYS APIs verified against the installed 2.0.1 packages and official 2.0.1 docs; data-format and effective-SINR choices verified against current docs + literature)

> **Scope note.** This stack covers the **offline tool** only. The "normal Simu5G build" gets **zero** new dependencies — no Python, no TensorFlow, no PyTorch, no GPU, no HDF5 unless we choose HDF5 for the artifact (see §What NOT to Use). Everything below lives in the sibling venv `/home/zoli/Projects/OMNET/Sionna/venv` and runs ahead of time. The C++ side only gains a small reader for one cached file.

---

## Headline findings (read these first)

1. **Sionna 2.x is NOT TensorFlow.** Sionna RT 2.0.1 was rewritten on **Dr.Jit + Mitsuba 3**; `sionna.phy` and `sionna.sys` run on **PyTorch**. The installed venv proves it: `tensorflow: NOT INSTALLED`, yet `import sionna.rt`, `import sionna.phy`, `import sionna.sys` all succeed (`torch==2.12.0`). **Do not** write code against any 0.x/1.x TensorFlow Sionna API — it does not exist here. This is the single biggest "don't trust training data" trap.

2. **The v1 "effective-SINR → reference curve" BLER method is a built-in Sionna feature: `sionna.sys.PHYAbstraction`.** It does exactly what the design plan §4.4 calls for — takes post-equalization SINR, applies **EESM** to get an effective SINR, looks up **shipped 5G-NR LDPC BLER tables** (interpolated over SINR × code-block-size per MCS), and returns BLER/TBLER. **Do not hand-roll EESM or hunt for external BLER curves.** Use `PHYAbstraction` (and optionally swap in `MIESM` via `sinr_effective_fun`). This removes the entire "where do reference curves come from" open question.

3. **Recommended internal pipeline (static, noise-limited v1):** `load_scene` → set `PlanarArray` arrays → add `Transmitter`/`Receiver` → `PathSolver` → `Paths.cfr(frequencies=subcarrier_frequencies(...))` → per-subcarrier post-eq SINR (noise-limited: from path gain + thermal noise + a chosen beamformer) → `PHYAbstraction(mcs_index, sinr=...)` → BLER per (link, MCS). Optionally use `RadioMapSolver`/`RadioMap.path_gain` for wide-area path-gain maps and as a calibration cross-check.

4. **Artifact format: HDF5 via `h5py` (already installed, 3.16.0).** Numeric N-dimensional `(link, MCS[, CQI])` table with rich attributes for the parameter contract + request hash. JSON is fine only for the tiny request/manifest sidecar. Binary-blob and JSON-for-bulk-data are rejected (see §What NOT to Use).

---

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

---

## Verified Sionna RT 2.x API surface (against installed 2.0.1)

All signatures below were introspected from the installed packages, not training data.

**Imports (matches `rt_step2.py`):**
```python
from sionna.rt import (load_scene, PlanarArray, Transmitter, Receiver,
                       PathSolver, RadioMapSolver, subcarrier_frequencies)
from sionna.sys import PHYAbstraction          # EESM + shipped 5G-NR BLER tables
```

**Scene + arrays + nodes** (static):
```python
scene = load_scene(sionna.rt.scene.munich, merge_shapes=True)   # merge_shapes speeds RT
scene.tx_array = PlanarArray(num_rows=1, num_cols=1, pattern="tr38901", polarization="V")
scene.rx_array = PlanarArray(num_rows=1, num_cols=1, pattern="dipole",  polarization="cross")
scene.add(Transmitter(name="tx", position=[...]));  scene.add(Receiver(name="rx", position=[...]))
```

**Path solving** — `PathSolver.__call__` verified signature:
```
(scene, max_depth=3, max_num_paths_per_src=1_000_000, samples_per_src=1_000_000,
 synthetic_array=True, los=True, specular_reflection=True, diffuse_reflection=False,
 refraction=True, diffraction=False, edge_diffraction=False,
 diffraction_lit_region=True, seed=42) -> Paths
```
```python
paths = PathSolver()(scene=scene, max_depth=5, los=True,
                     specular_reflection=True, refraction=True, seed=41)
```

**Channel extraction** — `Paths` public API (verified): `a` (complex path coeffs, shape `[num_rx, num_rx_ant, num_tx, num_tx_ant, num_paths]`), `tau`, `cir`, `cfr`, `taps`, `doppler`, plus geometry (`vertices`, `interactions`, `theta_t/r`, `phi_t/r`).
- **Sub-carrier frequencies:** `subcarrier_frequencies(num_subcarriers, subcarrier_spacing) -> Float`.
- **Channel frequency response (the key call for per-RB SINR):**
  `Paths.cfr(frequencies, sampling_frequency=1.0, num_time_steps=1, normalize_delays=True, normalize=False, reverse_direction=False, out_type='drjit'|'numpy'|'torch'|...)`.
  ```python
  freqs = subcarrier_frequencies(num_subcarriers=numBands*12, subcarrier_spacing=scs)
  H = paths.cfr(frequencies=freqs, normalize_delays=True, out_type="numpy")  # → ndarray
  ```
- **CIR (if path-level data wanted for the schema / future Doppler):** `Paths.cir(sampling_frequency=1.0, num_time_steps=1, normalize_delays=True, out_type=...)`.

**Radio maps (wide-area path gain / SINR, calibration + path-gain table):** `RadioMapSolver.__call__` returns a `RadioMap` exposing `path_gain`, `rss`, `sinr`, `cell_centers`, `tx_association`, `cdf`. Use `RadioMap.path_gain` to populate `getRSRP()` inputs and as an empty-world calibration anchor.

**Effective-SINR → BLER (Sionna SYS)** — `PHYAbstraction.__call__` verified signature:
```
(mcs_index: Tensor, sinr: Tensor|None=None, sinr_eff: Tensor|None=None,
 num_allocated_re: Tensor|None=None, mcs_table_index: int|Tensor=1,
 mcs_category: int|Tensor=0, check_mcs_index_validity=True)
 -> (num_decoded_bits, harq_feedback, sinr_eff, bler, tbler)
```
- Constructor loads + interpolates **shipped BLER tables** (`load_bler_tables_from='default'`) on a fine SINR×code-block-size grid per MCS; defaults: `MCSDecoderNR`, `TransportBlockNR`, EESM via `sinr_effective_fun`. Swap to **MIESM** by passing a different `sinr_effective_fun`.
- Pipeline: pass **per-subcarrier (post-equalization) SINR** + `mcs_index` (+ `mcs_table_index` for the 5G-NR MCS table) → get **BLER/TBLER** directly. This *is* the design plan's "RT → post-eq effective SINR → BLER via reference curves" — already implemented and calibrated.

---

## Chosen effective-SINR → BLER approach

**Decision: use `sionna.sys.PHYAbstraction` with its default EESM + shipped 5G-NR LDPC BLER tables.** Confidence: **HIGH**.

- **Why EESM/PHYAbstraction over hand-built curves:** EESM/MIESM are the standard, literature-validated link-to-system mappings for OFDM/5G-NR; `PHYAbstraction` ships calibrated reference tables and the MCS/transport-block machinery (38.211/212/214), so we get curves "for free," consistent with NR, without running our own LDPC Monte Carlo.
- **Why not full link-level LDPC Monte Carlo in v1:** higher fidelity but far costlier per (link, MCS) entry; deferred per PROJECT.md Out-of-Scope. `PHYAbstraction` is the cheap, fast-iteration path. (If higher fidelity is later wanted, `sionna.phy.nr` PUSCH/PDSCH chains exist in the same venv — clean upgrade, no new deps.)
- **x-axis contract:** `PHYAbstraction` consumes **post-equalization SINR**; the design plan §5 contract ("Simu5G's signal/(interference+noise) maps onto the curve's average-SNR x-axis") holds **iff** v1 builds curves under the same convention. v1 is noise-limited, so SINR=SNR and the mapping is direct. **Pin EESM-vs-MIESM and the `mcs_table_index` once, identically, on both sides.**
- **MIESM option:** more accurate for high-order QAM; available by passing `sinr_effective_fun=MIESM(...)`. Start with default EESM; switch if empty-world calibration shows bias.

---

## Chosen artifact / exchange format

**Decision: HDF5 (`h5py`) for the bulk numeric table + a tiny JSON manifest sidecar for the request/cache-key + parameter contract.** Confidence: **HIGH** for HDF5-vs-JSON-vs-binary; **MEDIUM** on exact C++ reader choice (HighFive vs raw HDF5 vs the binary variant).

### Schema sketch (v1, noise-limited: one BLER per (link, MCS), table holds all MCS/CQI per link)

```
artifact.h5
├── /attrs (parameter contract — asserted by SionnaChannelModel at init, fail loud on mismatch)
│     request_hash         : str   (sha256 of scene+positions+materials+antennas+freqs+powers+MCS set)
│     schema_version       : int
│     carrier_freq_hz      : float
│     subcarrier_spacing_hz: float
│     numerology / scs     : int
│     num_bands (RBs)      : int
│     bandwidth_hz         : float
│     tx_power_dbm         : float
│     polarization         : str
│     antenna_pattern_tx/rx: str
│     sinr_xaxis_def       : str   ("avg_snr_post_eq")
│     effective_sinr_method: str   ("EESM" | "MIESM")
│     mcs_table_index      : int
│     sionna_rt_version    : "2.0.1"
│     sionna_version       : "2.0.1"
├── /links
│     tx_id   : int32 [L]
│     rx_id   : int32 [L]
│     tx_pos  : float32 [L,3]    (Sionna scene coords)
│     rx_pos  : float32 [L,3]
│     coord_transform : attrs (origin, axes, units OMNeT++↔Sionna)
├── /mcs_index : int32 [M]       (or cqi axis)
├── /bler      : float32 [L, M]  (v1: scalar BLER per link×MCS; noise-limited)
├── /path_gain_db : float32 [L]  (per-link RT path gain → getRSRP / desired+interferer powers)
└── (v2 extension, additive) /sinr_grid : float32 [S]
    /bler_curve : float32 [L, M, S]   (BLER-vs-SINR curves; v1 is the S=fixed slice → nothing wasted)
```

- **Determinism/fingerprints:** commit/pin the `.h5`; record `request_hash` + library versions in attrs so reruns verify the cache and Sionna fingerprints stay reproducible regardless of GPU/float nondeterminism.
- **C++ reader:** prefer header-only **HighFive** to read HDF5 without dragging the heavy HDF5 C++ API into Simu5G; fall back to the raw HDF5 C lib. If adding *any* HDF5 to the Simu5G repo is unwanted, use the **binary+JSON variant** below.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `sionna.sys.PHYAbstraction` (EESM + shipped tables) | Full `sionna.phy.nr` PUSCH/PDSCH LDPC Monte Carlo | When v1's link-to-system fidelity is insufficient; same venv, no new deps — clean upgrade path. |
| EESM (PHYAbstraction default) | MIESM (`sinr_effective_fun=MIESM`) | When empty-world calibration shows bias at high-order QAM / high code rate. |
| HDF5 (`h5py`) artifact | NumPy `.npz` | Simpler if you accept weaker self-description and a NumPy-format C++ reader; HDF5's attributes + typing better fit the parameter contract. |
| `Paths.cfr` per-subcarrier H | `Paths.taps` (tapped-delay-line) | If a time-domain TDL is preferred for the effective-SINR computation; `cfr` is the direct frequency-domain route for per-RB SINR. |
| `PathSolver` per Tx/Rx batch | `RadioMapSolver` (`RadioMap.path_gain`) | Use radio maps for wide-area path-gain/coverage and as the empty-world calibration cross-check; use `PathSolver` for the precise per-link CIR/CFR feeding BLER. |
| GPU (CUDA via Dr.Jit) | CPU (LLVM via Dr.Jit) | CPU when no GPU is available; offline precompute tolerates slower CPU RT. |

---

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

---

## Stack Patterns by Variant

**If a GPU is available:**
- Run Dr.Jit on the CUDA backend; batch all Tx/Rx pairs in one `PathSolver`/`RadioMapSolver` call per scene (design plan §4.1 global precompute).
- Because GPU/TF-float results can be nondeterministic, **always pin the produced HDF5** as the reproducible source of truth.

**If CPU-only:**
- Dr.Jit uses the **LLVM** backend (must be present). RT is slower but the precompute-once model tolerates it. Reduce cost with `merge_shapes=True` and a modest `max_depth` (3–5).

**If the C++ team wants zero HDF5 dependency in Simu5G:**
- Emit a **versioned little-endian binary** dataset table + a **JSON manifest** (contract attrs + `request_hash`). C++ reads the manifest (assert/fail-loud), then mmaps/reads the binary `[L,M]` array. Keep HDF5 as the canonical/debug format in the offline tool.

**If interference matters (v2):**
- Switch the table from scalar `/bler[L,M]` to `/bler_curve[L,M,S]` over `/sinr_grid[S]`; Simu5G keeps owning the SINR value and looks up the curve. v1's scalar is the `S`-fixed slice — additive, nothing discarded.

---

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

**Compatibility caveat (MEDIUM confidence):** the installed venv is a single coherent set (sionna 2.0.1 / sionna-rt 2.0.1 / mitsuba 3.8.0 / drjit 1.3.1 / torch 2.12.0 / numpy 2.4.6) and is the source of truth — **pin exactly these** in a lockfile rather than re-resolving. Published NVIDIA docs sometimes still cite the TF (2.14–2.19) era for phy/sys; ignore that for 2.x — the actual installed phy/sys are PyTorch and TF-free.

---

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
