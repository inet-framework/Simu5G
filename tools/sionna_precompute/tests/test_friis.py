"""CAL-01: empty-world Sionna path gain must agree with the Friis free-space value.

This is the RED test of the TDD cycle: it imports ``compute_path_gain_dB`` from the
not-yet-written ``precompute`` module and asserts the Sionna empty-world LOS path gain
matches the textbook Friis value at the OMNeT++ Euclidean distance within 1.0 dB.

Observed in research (sionna-rt 2.0.1 venv): Sionna -83.36 dB vs Friis -83.32 dB at
100 m / 3.5 GHz -> residual ~0.04 dB, comfortably inside the gate.

The Sionna-dependent test is marked ``requires_venv`` so it can be deselected when the
active interpreter is not the offline venv, but it MUST run (not skip) under that venv.
"""
import math
import os
import sys

import pytest

# Make the tool dir importable regardless of pytest's rootdir.
_TOOL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOL_DIR not in sys.path:
    sys.path.insert(0, _TOOL_DIR)


def friis_dB(d, fc):
    """Free-space (Friis) path gain in dB at distance ``d`` (m), carrier ``fc`` (Hz)."""
    lam = 3e8 / fc
    return 20 * math.log10(lam / (4 * math.pi * d))


# A minimal SSOT mirroring scenario.example.json: one Tx, one Rx, 3.5 GHz, identity transform.
_SCENARIO = {
    "positions": [
        {"id": "tx", "role": "tx", "xyz_m": [0.0, 0.0, 10.0]},
        {"id": "rx", "role": "rx", "xyz_m": [100.0, 0.0, 1.5]},
    ],
    "antenna": {"num_rows": 1, "num_cols": 1, "pattern": "iso", "polarization": "V"},
    "carrier_frequency_hz": 3.5e9,
    "subcarrier_spacing_hz": 30e3,
    "numerology": 1,
    "num_bands": 1,
    "materials": [],
    "mcs_set": [0],
    "tx_power_convention": "eirp_dbm",
    "coord_transform": {
        "origin": [0.0, 0.0, 0.0],
        "axis_map": "identity",
        "scale": 1.0,
        "units": "m",
        "handedness": "right",
    },
}


@pytest.mark.requires_venv
def test_friis_round_trip():
    """Sionna empty-world path gain matches Friis within 1.0 dB at the OMNeT++ distance."""
    from precompute import compute_path_gain_dB

    sionna_dB = compute_path_gain_dB(_SCENARIO)

    # OMNeT++ Euclidean distance between the Tx and Rx positions (matches
    # phy_->getCoord().distance(coord) on the Simu5G side; ~100.36 m here).
    tx = _SCENARIO["positions"][0]["xyz_m"]
    rx = _SCENARIO["positions"][1]["xyz_m"]
    d = math.sqrt(sum((a - b) ** 2 for a, b in zip(tx, rx)))

    expected = friis_dB(d, _SCENARIO["carrier_frequency_hz"])
    assert abs(sionna_dB - expected) < 1.0, (
        f"Sionna {sionna_dB:.4f} dB vs Friis {expected:.4f} dB "
        f"(residual {abs(sionna_dB - expected):.4f} dB) exceeds 1.0 dB gate"
    )

    # Non-normalized semantics: absolute gain near -83 dB, NOT ~0 dB (which would
    # indicate normalize=True). Use the horizontal 100 m reference for the band.
    assert sionna_dB < -50.0, (
        f"path gain {sionna_dB:.4f} dB looks normalized (near 0); "
        "compute_path_gain_dB must use normalize=False"
    )
