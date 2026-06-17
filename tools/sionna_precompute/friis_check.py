#!/usr/bin/env python3
"""CAL-01 standalone cross-check harness.

Loads the SSOT, computes the empty-world Sionna path gain and the textbook Friis
free-space path gain at the OMNeT++ Euclidean distance, prints both plus the residual,
and exits nonzero if the residual is >= 1.0 dB. This is the standalone twin of the
pytest gate (tests/test_friis.py); both prove the TOOL-02 coord/units transform and the
dB link-budget convention are correct.

Usage:
    python friis_check.py scenario.example.json
"""
import math
import sys

from precompute import compute_path_gain_dB, load_scenario

TOLERANCE_DB = 1.0


def friis_dB(d, fc):
    """Free-space (Friis) path gain in dB at distance ``d`` (m), carrier ``fc`` (Hz)."""
    lam = 3e8 / fc
    return 20 * math.log10(lam / (4 * math.pi * d))


def _primary_link_distance(scenario):
    """OMNeT++ Euclidean distance for the first tx/rx pair (metres)."""
    txs = [p for p in scenario["positions"] if p["role"] == "tx"]
    rxs = [p for p in scenario["positions"] if p["role"] == "rx"]
    tx, rx = txs[0]["xyz_m"], rxs[0]["xyz_m"]
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(tx, rx)))


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print("usage: friis_check.py SCENARIO.json", file=sys.stderr)
        return 2

    scenario = load_scenario(argv[0])
    d = _primary_link_distance(scenario)
    fc = float(scenario["carrier_frequency_hz"])

    sionna_dB = compute_path_gain_dB(scenario)
    reference_dB = friis_dB(d, fc)
    residual = abs(sionna_dB - reference_dB)

    print(f"distance (OMNeT++ Euclidean): {d:.4f} m")
    print(f"carrier frequency:            {fc:.4e} Hz")
    print(f"Sionna path gain:             {sionna_dB:.4f} dB")
    print(f"Friis path gain:              {reference_dB:.4f} dB")
    print(f"residual:                     {residual:.4f} dB (gate < {TOLERANCE_DB} dB)")

    if residual >= TOLERANCE_DB:
        print("CAL-01 FAIL: residual exceeds tolerance", file=sys.stderr)
        return 1
    print("CAL-01 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
