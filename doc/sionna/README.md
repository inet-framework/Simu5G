# Sionna integration for Simu5G

This directory documents the **Sionna PHY integration** (Plan A: a site-specific,
ray-traced channel model). The channel is opt-in and does not affect the default
Simu5G build or behavior.

- [`sionna-integration-plan.md`](sionna-integration-plan.md) — umbrella overview (Plan A + Plan B)
- [`sionna-channel-plan.md`](sionna-channel-plan.md) — **Plan A**: the ray-traced channel (implemented)
- [`sionna-blercurves-plan.md`](sionna-blercurves-plan.md) — Plan B: BLER curves (design only)
- [`sionna-channel-implementation-decisions.md`](sionna-channel-implementation-decisions.md) — decisions taken during implementation
- [`cross-model-coupling-report.md`](cross-model-coupling-report.md) / [`.en.md`](cross-model-coupling-report.en.md) — the shadowing/fading cross-model coupling report
- [`code-review.md`](code-review.md) / [`.en.md`](code-review.en.md) — branch code review

Running an existing scenario with the Sionna channel is described separately, in
[`../../simulations/nr/sionna/README.md`](../../simulations/nr/sionna/README.md).

---

## Do you even need to install Sionna?

**Usually no.** The `SionnaManager` has three modes (see its NED documentation), and
only one of them needs Sionna:

| Mode | Needs Sionna? | Needs Python? |
|---|---|---|
| **Committed artifact** (`channelTableFile = "..."`) | no | no |
| **Live `tworay` backend** (deterministic two-ray) | no | yes — **standard library only** (any `python3`) |
| **Live `sionna` backend** (real ray tracing) | **yes** | yes — a Python with `sionna-rt` |

So Sionna RT is required **only** to run the generator with `backend = "sionna"`
(real, site-specific ray tracing). The committed `channel_table.json` artifact and
the deterministic `tworay` backend need no third-party dependency at all.

## Installing Sionna RT

Install the standalone **Sionna RT** package (`sionna-rt`), which is what the channel
generator uses. It is built on **Mitsuba 3 + Dr.Jit** and runs on CPU (LLVM backend)
or GPU (CUDA) — it does **not** pull in PyTorch (that is only needed by the full
`sionna` package, whose PHY layer we do not use here).

Install it into the **same Python** that `SionnaManager.pythonExecutable` points at
(a virtual environment is recommended). In this project that is the OMNeT++ venv,
e.g. `/home/…/omnetpp/.venv`:

```bash
# into your venv (recommended)
python3 -m pip install sionna-rt

# verify
python3 -c "import sionna.rt as rt; print('sionna.rt', rt.__version__)"
# -> sionna.rt 2.0.1   (Mitsuba 3 + Dr.Jit; CPU/LLVM, no GPU needed)
```

If you want the full library (PHY + RT) instead of just the ray tracer:

```bash
python3 -m pip install sionna        # full: also requires PyTorch
# or
python3 -m pip install sionna-no-rt  # PHY only, without ray tracing
```

### Requirements (from the official install guide)

- **Python 3.11+**.
- **OS:** Ubuntu 24.04 is the recommended platform.
- **CPU:** the Dr.Jit **LLVM** backend is required for CPU execution — see the Dr.Jit
  docs for the LLVM setup.
- **GPU (optional):** CUDA drivers; see the PyTorch/Mitsuba GPU setup.

### From source (optional, for development)

```bash
git clone --recursive https://github.com/NVlabs/sionna
pip install ext/sionna-rt/ .   # ray tracer
pip install .                  # full library
```

## Wiring Sionna into Simu5G

Once `sionna-rt` is installed in a Python, point the manager at it and select the
real ray tracer:

```ini
*.sionnaManager.channelTableFile = ""                 # generate (don't load an artifact)
*.sionnaManager.backend          = "sionna"           # real ray tracing
*.sionnaManager.pythonExecutable = "/path/to/venv/bin/python3"   # must import sionna.rt
# sionnaScript is left empty -> the bundled sionna_rt.py is found next to its NED
```

For a real geometry, also set `*.sionnaManager.sceneFile = "scene.xml"` (a Sionna-native
Mitsuba 3 scene); otherwise the flat-ground two-ray scene is used. Because ray-tracing
output can vary slightly across machines (float/GPU), **freeze the generated table and
commit it**, then load it via `channelTableFile` for reproducible/fingerprint-stable runs.

## Official references

- Sionna installation guide — https://nvlabs.github.io/sionna/installation.html
- Sionna documentation (home) — https://nvlabs.github.io/sionna/
- Introduction to Sionna RT (tutorial) — https://nvlabs.github.io/sionna/rt/tutorials/Introduction.html
- `sionna-rt` on PyPI — https://pypi.org/project/sionna-rt/
- `sionna` on PyPI — https://pypi.org/project/sionna/
- Sionna RT source (GitHub) — https://github.com/NVlabs/sionna-rt
- Sionna source (GitHub) — https://github.com/NVlabs/sionna
- Dr.Jit (LLVM/CUDA backends) — https://drjit.readthedocs.io/
- Mitsuba 3 — https://mitsuba.readthedocs.io/
- Sionna RT paper (arXiv 2303.11103) — https://arxiv.org/abs/2303.11103
