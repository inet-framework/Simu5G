# Sionna channel-table JSON contract (v1)

This file defines the data contract between Simu5G's `SionnaManager` (C++) and the
ray-tracing generator `sionna_rt.py` (Python). Two documents flow across the seam:

- **request** — written by `SionnaManager`, read by `sionna_rt.py`
- **table** (response) — written by `sionna_rt.py`, read back by `SionnaManager`

Both are plain JSON, versioned, human-readable and diffable. The `table` is the
*frozen artifact*: commit it for deterministic fingerprints (see Plan A §4.4).

All path gains are **power gains in dB**, spanning **Tx antenna port → Rx antenna
port** — i.e. they already include propagation + antenna patterns + beamforming
(Plan A §3). Therefore Simu5G must **not** re-apply antenna gain, cable loss, angular
attenuation, fading or shadowing on top; it adds only receiver-side noise.

Sign convention: `pathGainDb` is a gain (typically negative = loss). Simu5G forms
`recvPower_dBm = txPower_dBm + pathGainDb`. The legacy `getAttenuation()` value is
`-pathGainDb`.

---

## request

```jsonc
{
  "version": 1,
  "backend": "auto",                 // "auto" | "sionna" | "tworay"
  "scene": {
    "type": "flatGround",            // "flatGround" | "sceneFile"
    "sceneFile": "",                 // Mitsuba 3 XML path when type=="sceneFile"
    "groundPermittivity": 5.0,       // relative permittivity εr of the ground
    "groundConductivity": 0.001,     // σ [S/m]
    "sizeMeters": 2000.0,            // flat-ground plane half-extent
    "numReflections": 1              // ground reflections enabled (>=1)
  },
  "interferenceMode": "noiseLimited", // "noiseLimited" | "allPairs"
  "granularity": "perRb",            // "perRb" | "wideband"
  "polarization": "vertical",        // "vertical" | "horizontal"
  "carriers": [
    {
      "carrierFrequencyHz": 2.6e9,
      "numBands": 6,                 // number of RBs (logical bands)
      "numerology": 0               // SCS = 15kHz * 2^numerology; RB BW = 180kHz * 2^num
    }
  ],
  "nodes": [
    {
      "id": 1025,                    // MacNodeId (numeric, stable in a static run)
      "path": "Network.gnb1.cellularNic.nrPhy",  // OMNeT full path, for readability
      "role": "enb",                 // "enb" | "ue"
      "pos": [100.0, 200.0, 25.0],   // OMNeT Coord [x,y,z] in metres
      "antennaGainDb": 0.0           // isotropic by default in v1
    }
  ],
  "links": [                          // optional; if omitted, derived from interferenceMode
    { "tx": 1025, "rx": 1026 }
  ]
}
```

Link enumeration when `links` is omitted:
- `noiseLimited` → serving links only (each enb↔ue pair the caller marks; in v1 the
  caller always passes explicit `links`).
- `allPairs` → every Tx→Rx pair across all nodes for the carrier.

## table (response)

```jsonc
{
  "version": 1,
  "requestHash": "<hex>",            // hash of the request; SionnaManager caches by it
  "backend": "tworay",              // which backend actually produced this table
  "granularity": "perRb",
  "interferenceMode": "noiseLimited",
  "carriers": [
    {
      "carrierFrequencyHz": 2.6e9,
      "numBands": 6,
      "numerology": 0,
      "links": [
        {
          "tx": 1025,
          "rx": 1026,
          // perRb: length == numBands; wideband: length == 1
          "pathGainDb": [-92.1, -92.4, -92.6, -92.7, -92.9, -93.0],
          "rsrpDbm": null            // optional precomputed RSRP for the desired link
        }
      ]
    }
  ]
}
```

### Consistency rules
- `table.requestHash` must match the hash `SionnaManager` computed for the request
  (scene + node positions + materials + antennas + carriers + modes). Mismatch ⇒
  regenerate.
- `pathGainDb` length: `numBands` for `perRb`, `1` for `wideband`.
- `granularity == "wideband"` with `interferenceMode == "allPairs"` is flagged by
  `SionnaManager` (Plan A §8 coupling guard): a single wideband value cannot represent
  RB-selective interference.
- Doppler/temporal data is out of scope for v1 (time-invariant) but the schema may be
  extended with per-path fields later without breaking v1 readers.
