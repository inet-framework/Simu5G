#!/usr/bin/env python3
"""Offline Sionna precompute tool (Phase 1 walking skeleton, producer half).

Reads the shared scenario (SSOT), runs a single batched empty-world ``PathSolver`` over
the Tx/Rx links, extracts each per-link path gain via ``Paths.cfr`` over
``subcarrier_frequencies`` (normalize=False -> absolute gain), and emits the versioned
artifact set consumed by the Simu5G C++ side:

    results.h5      canonical/debug HDF5 (dataset /path_gain_dB + contract attrs)
    path_gain.bin   little-endian float64 [L] bulk table (NO floats ever go in JSON)
    manifest.json   schema_version, coord_transform, full parameter contract,
                    request_hash, degenerate sinr_grid (S=1), table descriptor

This tool runs ONLY in the offline Sionna venv. It shares no files with the Simu5G
build and adds no Python/TF/GPU dependency to a normal Simu5G build or run.

Usage:
    python precompute.py scenario.example.json [--out DIR]
"""
import argparse
import hashlib
import json
import math
import os
import sys

import numpy as np

SCHEMA_VERSION = 1
# v1 collapses the per-RB grid to one representative band-center subcarrier (Assumption A4).
SUBCARRIER_REPRESENTATION = "single-subcarrier-band-center"

# Pinned offline-tool library versions (mirror requirements.txt) recorded in the
# request_hash + manifest for reproducibility / cache invalidation.
_LIB_VERSIONS = {
    "sionna-rt": "2.0.1",
    "sionna": "2.0.1",
    "numpy": "2.4.6",
    "h5py": "3.16.0",
}


# --------------------------------------------------------------------------- #
# SSOT loading + validation (T-01-01: fail before running RT on malformed input)
# --------------------------------------------------------------------------- #
def _require(cond, msg):
    if not cond:
        raise ValueError(f"Invalid scenario: {msg}")


def validate_scenario(scenario):
    """Validate required keys/types in the SSOT. Raise ValueError on any problem.

    Mitigates T-01-01 (tampered/malformed SSOT) by failing loudly before invoking RT.
    """
    _require(isinstance(scenario, dict), "top level must be an object")

    pos = scenario.get("positions")
    _require(isinstance(pos, list) and len(pos) >= 2,
             "'positions' must be a list with >= 2 entries (>=1 tx, >=1 rx)")
    roles = set()
    for i, p in enumerate(pos):
        _require(isinstance(p, dict), f"positions[{i}] must be an object")
        _require(p.get("role") in ("tx", "rx"), f"positions[{i}].role must be 'tx' or 'rx'")
        xyz = p.get("xyz_m")
        _require(isinstance(xyz, list) and len(xyz) == 3
                 and all(isinstance(c, (int, float)) for c in xyz),
                 f"positions[{i}].xyz_m must be a list of 3 numbers")
        _require(isinstance(p.get("id"), str) and p["id"], f"positions[{i}].id must be a non-empty string")
        roles.add(p["role"])
    _require("tx" in roles and "rx" in roles, "scenario needs at least one tx and one rx")

    ant = scenario.get("antenna")
    _require(isinstance(ant, dict), "'antenna' must be an object")
    for k in ("num_rows", "num_cols", "pattern", "polarization"):
        _require(k in ant, f"antenna.{k} is required")
    _require(isinstance(ant["num_rows"], int) and ant["num_rows"] >= 1, "antenna.num_rows must be int >= 1")
    _require(isinstance(ant["num_cols"], int) and ant["num_cols"] >= 1, "antenna.num_cols must be int >= 1")

    _require(isinstance(scenario.get("carrier_frequency_hz"), (int, float))
             and scenario["carrier_frequency_hz"] > 0, "carrier_frequency_hz must be a positive number")
    _require(isinstance(scenario.get("subcarrier_spacing_hz"), (int, float))
             and scenario["subcarrier_spacing_hz"] > 0, "subcarrier_spacing_hz must be a positive number")
    _require(isinstance(scenario.get("num_bands"), int) and scenario["num_bands"] >= 1,
             "num_bands must be int >= 1")
    _require(isinstance(scenario.get("mcs_set"), list) and len(scenario["mcs_set"]) >= 1,
             "mcs_set must be a non-empty list")
    _require(isinstance(scenario.get("tx_power_convention"), str) and scenario["tx_power_convention"],
             "tx_power_convention must be a non-empty string")

    ct = scenario.get("coord_transform")
    _require(isinstance(ct, dict), "'coord_transform' (TOOL-02) is required")
    for k in ("origin", "axis_map", "scale", "units", "handedness"):
        _require(k in ct, f"coord_transform.{k} is required")
    _require(isinstance(ct["origin"], list) and len(ct["origin"]) == 3,
             "coord_transform.origin must be a list of 3 numbers")

    return scenario


def load_scenario(path):
    """Load and validate the SSOT JSON at ``path``."""
    with open(path, "r") as f:
        scenario = json.load(f)
    return validate_scenario(scenario)


# --------------------------------------------------------------------------- #
# Coord transform (TOOL-02): OMNeT++ coords -> Sionna scene coords. v1 = identity.
# --------------------------------------------------------------------------- #
def apply_coord_transform(xyz, coord_transform):
    """Map an OMNeT++ position into Sionna scene coordinates.

    v1 supports the identity axis map with an explicit origin offset + uniform scale,
    exactly as recorded in the manifest. Unknown axis maps fail loudly.
    """
    axis_map = coord_transform.get("axis_map", "identity")
    _require(axis_map == "identity", f"unsupported coord_transform.axis_map '{axis_map}' (v1: identity only)")
    origin = coord_transform["origin"]
    scale = float(coord_transform["scale"])
    return [(float(c) - float(o)) * scale for c, o in zip(xyz, origin)]


def _euclidean(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


# --------------------------------------------------------------------------- #
# Empty-world path gain (TOOL-03, RESEARCH Pattern 4, VERIFIED against the venv)
# --------------------------------------------------------------------------- #
def _link_pairs(scenario):
    """Return ordered (tx_pos, rx_pos) link pairs. v1: each rx against the first tx."""
    txs = [p for p in scenario["positions"] if p["role"] == "tx"]
    rxs = [p for p in scenario["positions"] if p["role"] == "rx"]
    tx = txs[0]
    return [(tx["xyz_m"], rx["xyz_m"]) for rx in rxs]


def compute_path_gain_dB(scenario):
    """Empty-world LOS path gain (dB) for the scenario's primary link.

    Returns a single float for the first tx/rx pair (the v1 single-link skeleton).
    Mirrors RESEARCH Pattern 4 exactly: empty world, iso antennas, max_depth=0,
    normalize=False (absolute gain), path gain = 10*log10(mean(|H|^2)).
    """
    gains = compute_path_gains_dB(scenario)
    return gains[0]


def compute_path_gains_dB(scenario):
    """Empty-world LOS path gain (dB) for every link in the scenario.

    One batched PathSolver call per link (v1 has a single link; the structure is
    ready for the [L] table). NEVER overrides the Mitsuba variant; always
    synthetic_array=False; normalize=False is mandatory (Pitfall 4).
    """
    # Imported lazily so the module (and load_scenario / validation) import without
    # the heavy RT stack present.
    from sionna.rt import (load_scene, PlanarArray, Transmitter, Receiver,
                           PathSolver, subcarrier_frequencies)

    validate_scenario(scenario)
    ct = scenario["coord_transform"]
    ant = scenario["antenna"]
    fc = float(scenario["carrier_frequency_hz"])
    scs = float(scenario["subcarrier_spacing_hz"])

    gains = []
    for tx_xyz, rx_xyz in _link_pairs(scenario):
        scene = load_scene()  # empty world (no geometry)
        scene.frequency = fc
        scene.tx_array = PlanarArray(num_rows=ant["num_rows"], num_cols=ant["num_cols"],
                                     pattern=ant["pattern"], polarization=ant["polarization"])
        scene.rx_array = PlanarArray(num_rows=ant["num_rows"], num_cols=ant["num_cols"],
                                     pattern=ant["pattern"], polarization=ant["polarization"])
        scene.add(Transmitter("tx", apply_coord_transform(tx_xyz, ct)))
        scene.add(Receiver("rx", apply_coord_transform(rx_xyz, ct)))

        paths = PathSolver()(scene=scene, max_depth=0, los=True,
                             specular_reflection=False, refraction=False,
                             synthetic_array=False)
        freqs = subcarrier_frequencies(1, scs)  # v1 single representative subcarrier
        H = paths.cfr(frequencies=freqs, normalize=False, out_type="numpy")
        pathgain_lin = float(np.mean(np.abs(H) ** 2))
        gains.append(10.0 * math.log10(pathgain_lin))
    return gains


# --------------------------------------------------------------------------- #
# Request hash (T-01-02: cache/integrity key only, NOT tamper protection)
# --------------------------------------------------------------------------- #
def compute_request_hash(scenario):
    """SHA-256 hex of the canonicalized SSOT + pinned library versions.

    Used as a cache key / integrity check ONLY. The C++ side re-asserts the full
    parameter contract; this hash is not a security control (V6 N/A).
    """
    payload = {"scenario": scenario, "lib_versions": _LIB_VERSIONS}
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


# --------------------------------------------------------------------------- #
# Artifact emission (ART-01, ART-02): HDF5 canonical + LE-binary table + manifest
# --------------------------------------------------------------------------- #
def build_manifest(scenario, gains_dB):
    """Assemble the manifest dict (small scalars/lists only; bulk floats go to binary)."""
    return {
        "schema_version": SCHEMA_VERSION,
        "coord_transform": scenario["coord_transform"],
        "carrier_frequency_hz": scenario["carrier_frequency_hz"],
        "subcarrier_spacing_hz": scenario["subcarrier_spacing_hz"],
        "num_bands": scenario["num_bands"],
        "antenna": scenario["antenna"],
        "tx_power_convention": scenario["tx_power_convention"],
        "polarization": scenario["antenna"]["polarization"],
        "request_hash": compute_request_hash(scenario),
        "num_links": len(gains_dB),
        "subcarrier_representation": SUBCARRIER_REPRESENTATION,
        "table_path": "path_gain.bin",
        "table_dtype": "<f8",
        # Degenerate (S=1) SINR axis: v2's interference curves become a purely
        # additive [L, S] extension over this grid (ART-02).
        "sinr_grid": [0.0],
        "lib_versions": _LIB_VERSIONS,
    }


def write_artifact(scenario, gains_dB, out_dir):
    """Write results.h5 + path_gain.bin + manifest.json into ``out_dir``."""
    os.makedirs(out_dir, exist_ok=True)
    arr = np.asarray(gains_dB, dtype="<f8")  # little-endian float64 [L]

    # (1) Bulk table: little-endian float64 binary [L]. NEVER in JSON (T-01-03).
    bin_path = os.path.join(out_dir, "path_gain.bin")
    arr.astype("<f8").tofile(bin_path)

    manifest = build_manifest(scenario, gains_dB)

    # (2) Canonical/debug HDF5 with the contract attrs alongside the array.
    import h5py
    h5_path = os.path.join(out_dir, "results.h5")
    with h5py.File(h5_path, "w") as h5:
        dset = h5.create_dataset("path_gain_dB", data=arr)
        for k, v in manifest.items():
            if isinstance(v, (dict, list)):
                dset.attrs[k] = json.dumps(v, sort_keys=True)
            else:
                dset.attrs[k] = v

    # (3) Small JSON manifest sidecar (scalars + small lists only).
    manifest_path = os.path.join(out_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, sort_keys=True, indent=2)

    return {"h5": h5_path, "bin": bin_path, "manifest": manifest_path}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Sionna empty-world precompute tool")
    parser.add_argument("scenario", help="path to the SSOT scenario JSON")
    parser.add_argument("--out", default="sionna_artifact", help="output directory for the artifact set")
    args = parser.parse_args(argv)

    scenario = load_scenario(args.scenario)
    gains_dB = compute_path_gains_dB(scenario)
    out = write_artifact(scenario, gains_dB, args.out)

    print(f"path gain dB: {gains_dB}")
    print(f"wrote: {out['h5']}, {out['bin']}, {out['manifest']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
