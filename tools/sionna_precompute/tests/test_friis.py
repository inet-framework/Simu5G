"""CAL-01: empty-world Sionna path gain must agree with the Friis free-space value.

This is the RED test of the TDD cycle: it imports ``compute_path_gain_dB`` from the
precompute module and asserts the Sionna empty-world LOS path gain matches the textbook
Friis value at the OMNeT++ **3D Euclidean distance** within 0.25 dB.

The Tx is at [0, 0, 10] m and the Rx is at [100, 0, 1.5] m, giving a 3D Euclidean
distance of ~100.36 m (NOT the horizontal 100 m projection). Both this test and
friis_check.py use the 3D distance — matching phy_->getCoord().distance(coord) on
the Simu5G side — so the comparison is apples-to-apples (WR-07).

Observed in research (sionna-rt 2.0.1 venv): Sionna -83.36 dB vs Friis -83.32 dB at
~100.36 m / 3.5 GHz -> residual ~0.04 dB, well inside the 0.25 dB gate.

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
    """Sionna empty-world path gain matches Friis within 0.25 dB at the OMNeT++ 3D distance."""
    from precompute import compute_path_gain_dB

    sionna_dB = compute_path_gain_dB(_SCENARIO)

    # 3D Euclidean distance between Tx [0,0,10] and Rx [100,0,1.5] (metres).
    # This matches phy_->getCoord().distance(coord) on the Simu5G side (~100.36 m here).
    # NOTE: use the 3D distance, NOT the horizontal 100 m projection — the height
    # difference matters and both sides must use the same convention (WR-07).
    tx = _SCENARIO["positions"][0]["xyz_m"]
    rx = _SCENARIO["positions"][1]["xyz_m"]
    d = math.sqrt(sum((a - b) ** 2 for a, b in zip(tx, rx)))

    expected = friis_dB(d, _SCENARIO["carrier_frequency_hz"])
    assert abs(sionna_dB - expected) < 0.25, (
        f"Sionna {sionna_dB:.4f} dB vs Friis {expected:.4f} dB "
        f"(residual {abs(sionna_dB - expected):.4f} dB) exceeds 0.25 dB gate"
    )

    # Non-normalized semantics: absolute gain near -83 dB, NOT ~0 dB (which would
    # indicate normalize=True was used). The 3D Euclidean distance at 3.5 GHz gives
    # Friis ≈ -83.32 dB; Sionna observed ≈ -83.36 dB.
    assert sionna_dB < -50.0, (
        f"path gain {sionna_dB:.4f} dB looks normalized (near 0); "
        "compute_path_gain_dB must use normalize=False"
    )
