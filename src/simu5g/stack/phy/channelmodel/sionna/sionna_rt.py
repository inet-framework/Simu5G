#!/usr/bin/env python3
#
#                  Simu5G
#
# Copyright (C) 2012-2026 Giovanni Nardini, Giovanni Stea et al. (University of Pisa)
#
# This file is part of a software released under the license included in file
# "license.pdf". Please read LICENSE and README files before using it.
#
"""Sionna RT channel-table generator for Simu5G (Plan A, v1).

Reads a *request* JSON describing a static scenario (node positions, antennas,
carriers, ground material, scene) and writes a *table* JSON of per-(link, RB)
path gains in dB, spanning Tx port -> Rx port. See SCHEMA.md for the contract.

Two backends:

* ``tworay`` -- a self-contained, deterministic two-ray ground-reflection model
  (direct path + single ground reflection). No third-party dependency. This is
  the reference backend used for committed, fingerprint-stable artifacts; it is
  exactly the "flat ground, direct + ground-reflected" model of Plan A Sec. 5.

* ``sionna`` -- NVIDIA Sionna RT (Mitsuba 3 / Dr.Jit, CPU LLVM backend). Used for
  real scenes (``scene.type == "sceneFile"``) and as the physically grounded
  engine. Requires ``import sionna.rt`` to succeed.

``auto`` picks ``sionna`` if importable, else ``tworay``.

Usage:
    sionna_rt.py <request.json> <out_table.json>
"""

import sys
import json
import math
import cmath

C0 = 299792458.0          # speed of light [m/s]
EPS0 = 8.8541878128e-12   # vacuum permittivity [F/m]


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #
def rb_bandwidth_hz(numerology):
    """3GPP NR resource-block bandwidth: 12 subcarriers * (15 kHz * 2^mu)."""
    return 12.0 * 15.0e3 * (2.0 ** numerology)


def band_center_frequencies(carrier):
    """Per-RB center frequencies, centered on the carrier frequency.

    Returns a list of length numBands (perRb) or [fc] semantics handled by caller.
    """
    fc = float(carrier["carrierFrequencyHz"])
    n = int(carrier["numBands"])
    bw = rb_bandwidth_hz(int(carrier.get("numerology", 0)))
    # RB-center offsets must match the Sionna backend's subcarrier_frequencies(),
    # i.e. the canonical OFDM/FFT grid (arange(n) - n//2)*bw, so band index i maps
    # to the same absolute frequency in both backends (matters for even n).
    return [fc + (i - n // 2) * bw for i in range(n)]


def lin_to_db(x):
    if not math.isfinite(x) or x <= 0.0:
        return -300.0
    return 10.0 * math.log10(x)


# --------------------------------------------------------------------------- #
# two-ray ground-reflection backend (deterministic)
# --------------------------------------------------------------------------- #
def _complex_permittivity(eps_r, sigma, freq_hz):
    """Relative complex permittivity: eps_r - j*sigma/(omega*eps0)."""
    omega = 2.0 * math.pi * freq_hz
    return complex(eps_r, -sigma / (omega * EPS0))


def _reflection_coeff(eps_c, sin_theta, cos_theta, polarization):
    """Fresnel reflection coefficient for a grazing angle theta from the ground.

    sin_theta / cos_theta describe the angle between the reflected ray and the
    ground plane (grazing angle).
    """
    root = cmath.sqrt(eps_c - cos_theta * cos_theta)
    if polarization == "horizontal":
        return (sin_theta - root) / (sin_theta + root)
    # vertical (default)
    return (eps_c * sin_theta - root) / (eps_c * sin_theta + root)


def _two_ray_path_gain_db(tx, rx, freq_hz, eps_r, sigma, polarization,
                          gain_tx_db, gain_rx_db):
    """Coherent two-ray power gain [dB], Tx port -> Rx port (incl. antenna gains)."""
    dx = tx["pos"][0] - rx["pos"][0]
    dy = tx["pos"][1] - rx["pos"][1]
    ht = max(tx["pos"][2], 1e-3)
    hr = max(rx["pos"][2], 1e-3)
    d = math.hypot(dx, dy)            # horizontal separation

    lam = C0 / freq_hz
    k = 2.0 * math.pi / lam

    # floor the path lengths so coincident Tx/Rx (d=0, ht=hr) does not divide by zero
    d_los = max(math.sqrt(d * d + (ht - hr) ** 2), 1e-3)
    d_ref = max(math.sqrt(d * d + (ht + hr) ** 2), 1e-3)

    # grazing angle of the reflected ray
    sin_theta = (ht + hr) / d_ref
    cos_theta = d / d_ref
    eps_c = _complex_permittivity(eps_r, sigma, freq_hz)
    gamma = _reflection_coeff(eps_c, sin_theta, cos_theta, polarization)

    # coherent sum of direct + ground-reflected field (isotropic field ~ 1/d)
    field = (cmath.exp(-1j * k * d_los) / d_los
             + gamma * cmath.exp(-1j * k * d_ref) / d_ref)
    # free-space constant (lambda / 4 pi)^2, with field normalized to amplitude 1/d.
    # Clamp to <= 1 (0 dB): passive propagation cannot amplify, and this bounds the
    # degenerate near-coincident case (only reached for sub-metre separations).
    power_gain = min((lam / (4.0 * math.pi)) ** 2 * (abs(field) ** 2), 1.0)

    return lin_to_db(power_gain) + gain_tx_db + gain_rx_db


def _gain_db(node):
    return float(node.get("antennaGainDb", 0.0))


def run_tworay(request):
    return _run_generic(request, _two_ray_link_gains)


def _two_ray_link_gains(request, carrier, tx, rx, freqs):
    eps_r = float(request["scene"].get("groundPermittivity", 5.0))
    sigma = float(request["scene"].get("groundConductivity", 0.001))
    pol = request.get("polarization", "vertical")
    gtx, grx = _gain_db(tx), _gain_db(rx)
    return [_two_ray_path_gain_db(tx, rx, f, eps_r, sigma, pol, gtx, grx)
            for f in freqs]


# --------------------------------------------------------------------------- #
# Sionna RT backend (real ray tracing) -- wired to Sionna RT 2.x
# --------------------------------------------------------------------------- #
def _flat_ground_xml(eps_r, sigma, half_extent):
    """Mitsuba 3 scene: a single large rectangle at z=0 with a radio material."""
    return ("""<?xml version="1.0"?>
<scene version="2.1.0">
  <bsdf type="radio-material" id="ground">
    <float name="relative_permittivity" value="%g"/>
    <float name="conductivity" value="%g"/>
  </bsdf>
  <shape type="rectangle">
    <ref id="ground"/>
    <transform name="to_world">
      <scale x="%g" y="%g" z="1"/>
    </transform>
  </shape>
</scene>""" % (eps_r, sigma, half_extent, half_extent))


def _pol_letter(request):
    return "H" if request.get("polarization") == "horizontal" else "V"


def run_sionna(request):
    import numpy as np
    import sionna.rt as rt
    from sionna.rt import (load_scene, load_scene_from_string, PlanarArray,
                           Transmitter, Receiver, PathSolver,
                           subcarrier_frequencies)

    scene_cfg = request["scene"]
    if scene_cfg.get("type") == "sceneFile" and scene_cfg.get("sceneFile"):
        # external Mitsuba scene; node positions must already be in its frame
        scene = load_scene(scene_cfg["sceneFile"])
    else:
        eps_r = float(scene_cfg.get("groundPermittivity", 5.0))
        sigma = float(scene_cfg.get("groundConductivity", 0.001))
        half = float(scene_cfg.get("sizeMeters", 2000.0))
        scene = load_scene_from_string(_flat_ground_xml(eps_r, sigma, half))

    pol = _pol_letter(request)
    scene.tx_array = PlanarArray(num_rows=1, num_cols=1, pattern="iso", polarization=pol)
    scene.rx_array = PlanarArray(num_rows=1, num_cols=1, pattern="iso", polarization=pol)

    num_refl = int(scene_cfg.get("numReflections", 1))
    max_depth = max(1, num_refl + 1)
    solver = PathSolver()

    by_id = {n["id"]: n for n in request["nodes"]}
    granularity = request.get("granularity", "perRb")
    out_carriers = []
    for carrier in request["carriers"]:
        fc = float(carrier["carrierFrequencyHz"])
        n_bands = int(carrier["numBands"])
        scene.frequency = fc
        if granularity == "wideband":
            freqs = subcarrier_frequencies(1, 0.0)
            n_out = 1
        else:
            scs = rb_bandwidth_hz(int(carrier.get("numerology", 0)))
            freqs = subcarrier_frequencies(n_bands, scs)  # offsets around fc
            n_out = n_bands

        links_out = []
        for (tx_id, rx_id) in _enumerate_links(request, request["nodes"]):
            tx, rx = by_id[tx_id], by_id[rx_id]
            # reset radio devices for this link
            for nm in list(scene.transmitters.keys()):
                scene.remove(nm)
            for nm in list(scene.receivers.keys()):
                scene.remove(nm)
            scene.add(Transmitter("tx", position=[float(c) for c in tx["pos"]]))
            scene.add(Receiver("rx", position=[float(c) for c in rx["pos"]]))

            paths = solver(scene, max_depth=max_depth, los=True,
                           specular_reflection=True, diffraction=False,
                           refraction=False, seed=42)
            H = np.asarray(paths.cfr(frequencies=freqs, normalize=False,
                                     out_type="numpy")).squeeze()
            power = np.atleast_1d(np.abs(H) ** 2)
            gtx, grx = _gain_db(tx), _gain_db(rx)
            gains = [(10.0 * math.log10(p) if p > 0.0 else -300.0) + gtx + grx
                     for p in power.ravel()[:n_out]]
            links_out.append({"tx": tx_id, "rx": rx_id,
                              "pathGainDb": gains, "rsrpDbm": None})
        out_carriers.append({
            "carrierFrequencyHz": fc,
            "numBands": n_bands,
            "numerology": int(carrier.get("numerology", 0)),
            "links": links_out,
        })
    return out_carriers


# --------------------------------------------------------------------------- #
# shared driver
# --------------------------------------------------------------------------- #
def _enumerate_links(request, carrier_nodes):
    if request.get("links"):
        return [(l["tx"], l["rx"]) for l in request["links"]]
    mode = request.get("interferenceMode", "noiseLimited")
    ids = [n["id"] for n in carrier_nodes]
    if mode == "allPairs":
        return [(tx, rx) for tx in ids for rx in ids if tx != rx]
    # noiseLimited with no explicit links: enb -> ue pairs
    enbs = [n["id"] for n in carrier_nodes if n.get("role") == "enb"]
    ues = [n["id"] for n in carrier_nodes if n.get("role") == "ue"]
    return [(e, u) for e in enbs for u in ues]


def _run_generic(request, link_gain_fn):
    by_id = {n["id"]: n for n in request["nodes"]}
    granularity = request.get("granularity", "perRb")
    out_carriers = []
    for carrier in request["carriers"]:
        if granularity == "wideband":
            freqs = [float(carrier["carrierFrequencyHz"])]
        else:
            freqs = band_center_frequencies(carrier)

        links_out = []
        for (tx_id, rx_id) in _enumerate_links(request, request["nodes"]):
            tx, rx = by_id[tx_id], by_id[rx_id]
            gains = link_gain_fn(request, carrier, tx, rx, freqs)
            links_out.append({
                "tx": tx_id,
                "rx": rx_id,
                "pathGainDb": gains,
                "rsrpDbm": None,
            })
        out_carriers.append({
            "carrierFrequencyHz": float(carrier["carrierFrequencyHz"]),
            "numBands": int(carrier["numBands"]),
            "numerology": int(carrier.get("numerology", 0)),
            "links": links_out,
        })
    return out_carriers


def select_backend(request):
    backend = request.get("backend", "auto")
    if backend == "tworay":
        return "tworay"
    if backend == "sionna":
        return "sionna"
    # auto
    try:
        import sionna.rt  # noqa: F401
        return "sionna"
    except Exception:
        return "tworay"


def generate(request):
    backend = select_backend(request)
    if backend == "sionna":
        try:
            carriers = run_sionna(request)
        except NotImplementedError:
            backend = "tworay"
            carriers = run_tworay(request)
    else:
        carriers = run_tworay(request)
    return {
        "version": 1,
        "requestHash": request.get("requestHash", ""),
        "backend": backend,
        "granularity": request.get("granularity", "perRb"),
        "interferenceMode": request.get("interferenceMode", "noiseLimited"),
        "carriers": carriers,
    }


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: sionna_rt.py <request.json> <out_table.json>\n")
        return 2
    with open(argv[1]) as f:
        request = json.load(f)
    table = generate(request)
    with open(argv[2], "w") as f:
        # allow_nan=False: never emit the non-standard Infinity/NaN tokens (which a
        # strict JSON reader rejects); fail loudly instead if anything slips through.
        json.dump(table, f, indent=2, allow_nan=False)
        f.write("\n")
    sys.stderr.write("sionna_rt.py: wrote %d carrier(s) using backend '%s'\n"
                     % (len(table["carriers"]), table["backend"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
