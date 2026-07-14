<!--
  DRAFT entry for WHATSNEW.md, covering the NR sidelink (PC5) work.
  Not yet merged into WHATSNEW.md: the release/version heading under which this
  belongs is the maintainer's call. Placeholder heading below is "Upcoming
  release"; replace with the actual "## vX.Y.Z (date)" line at release time and
  prepend the section to WHATSNEW.md.
-->

## Upcoming release

### NR sidelink (PC5) support — draft WHATSNEW section

Simu5G gains a spec-based implementation of 3GPP NR sidelink (PC5), covering
a Rel-16 subset referred to internally as phase "SL-1": out-of-coverage
broadcast V2X. It ships as an optional package under
`src/simu5g/stack/sidelink/`, activated per UE with the `NrUe.hasSidelink`
parameter (default `false`; when off, zero sidelink modules are instantiated).
Inside the NIC (`NrNicUe`), the sidelink leg (`slRrc` / `slRlcMux` / `slMac`
/ `slPhy` / `slChannelModel`) is a third protocol leg alongside the existing
LTE and NR Uu legs. It is unrelated to the pre-existing, non-3GPP "D2D"
research prototype (`simu5g.stack.d2d`), which remains a separate feature.

#### Features

- **Out-of-coverage operation.** Sidelink UEs can run with no serving cell
  (`servingNodeId` / `nrServingNodeId` = 0). The new `SlUe` preset node
  (extends `NrUe`) enables the sidelink leg by default, for pure PC5-only
  scenarios.

- **Mode-2 (UE-autonomous) sensing-based resource selection**, per TS 38.321
  clause 5.22.1.1: candidate single-slot resources are excluded based on
  sensed SCI reservations, with an SPS reselection counter and a
  `probResourceKeep` probability of keeping the current resource on
  expiry. Two alternative modes share the same MAC
  (`NrSlMacUe.resourceAllocationMode`): `random` (same selection window,
  no sensing — the standard baseline) and `static` (one fixed,
  preconfigured periodic grant per UE).

- **Broadcast PC5 traffic** addressed with 24-bit Layer-2 source/destination
  IDs. Sidelink radio bearers (SLRBs) are statically configured (UM RLC) via
  the `SlRrc.preconfig` JSON parameter (resource pool geometry, carrier
  frequency, numerology, sensing thresholds, SLRB-to-destination mapping).
  The PC5-RRC control plane is "genie" style — driven by C++ calls rather
  than over-the-air signaling.

- **Blind HARQ retransmissions**: up to 16 sidelink HARQ processes, with
  receiver-side soft combining of repeated copies and duplicate-delivery
  suppression keyed on (source, HARQ process, NDI).

- **TR 37.885 V2V channel model** (`Tr37885ChannelModel`): highway/urban
  pathloss, per-pair log-normal shadowing, optional NLOSv vehicle-blockage
  loss, and SINR with co-slot/co-subchannel interference drawn from an SL
  transmission map; decoding is a PSCCH (SCI) SINR threshold followed by
  PSSCH BLER decoding against per-CQI curves. A parameter-free
  `SlIdealChannelModel` (deterministic reception) is the default.

- **Measurements/statistics**: SL-RSRP, SL SINR, channel busy ratio (CBR,
  per TS 38.215, abstracted) and frame loss as PHY signals/statistics; an
  optional network-level `SlStatsCollector` submodule aggregates PRR/PIR
  per transmitter-receiver distance bin as end-of-run scalars, per TR
  37.885 clause 6.1.5.

- **Event-driven design.** No per-slot ticker: sensing state updates on SCI
  reception, and transmit self-events fire only on the UE's own grant slots.

- **Examples.** `simulations/nr/sidelink/basic`: a two-UE out-of-coverage
  pipe (sidelink-disabled check, ideal-channel broadcast, TR 37.885
  broadcast with a PER-vs-distance sweep, a 50-UE mode-2-vs-random
  sensing-gain comparison, and a blind-HARQ-retransmission case).
  `simulations/nr/sidelink/highway`: a TR 37.885 highway calibration
  scenario (170 vehicles, 2 km / 6-lane, 100 km/h, CAM-style 300B
  broadcasts at 10 Hz), plus `Highway-Small`/`Highway-300` variants;
  measured PRR ~0.99 at short range down to ~0.97 at 500 m. Unit tests are
  under `tests/unit/sidelink`; regression rows in
  `tests/fingerprint/sidelink.csv`.

#### Scope and limitations (SL-1)

This is a first phase, intentionally scoped to broadcast-only out-of-coverage
operation:

- Only broadcast PC5 traffic is supported; unicast/groupcast PC5-RRC,
  PSFCH-based HARQ feedback, SL-SDAP/QoS handling, and CBR-based congestion
  control are planned for a follow-up phase ("SL-2").
- Mode 1 (gNB-scheduled sidelink resources) is not yet supported; only
  Mode 2 (UE-autonomous), `random`, and `static` allocation are available.
- Resources are single-slot; there is no multi-slot/multi-resource grant
  chaining.
- MCS is used directly as the CQI index for the BLER lookup, and the
  transmit-opportunity size (`tbSize`) is a plain byte-count parameter —
  there is no MCS-to-TBS link-adaptation model yet.
- Synchronization is ideal; Rel-16 resource re-evaluation/pre-emption and
  Rel-17 inter-UE coordination are not modeled.
