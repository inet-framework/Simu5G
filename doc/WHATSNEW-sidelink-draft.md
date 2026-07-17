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

### Phase SL-2: unicast, PSFCH feedback, QoS and congestion control

The second phase upgrades the broadcast-only SL-1 into a usable Rel-16
subset:

- **Link adaptation.** A self-contained `SlMcsTable` (TS 38.214 §5.1.3
  abstraction) replaces the SL-1 stubs: the transport block size is
  computed from the grant's MCS (table 1) and width (`tbSize = -1B`
  default; explicit values remain as an escape hatch), and the BLER
  lookup uses a coarse rule-derived CQI equivalent of the MCS.

- **PC5 unicast links.** Unicast packets to SL-capable peers are
  classified onto the sidelink by a static rule (`pc5UnicastEnabled`);
  the first packet triggers PC5-RRC link establishment with per-link
  SLRBs from the `unicastSlrbDefaults` templates (UM or AM — AM STATUS
  PDUs get their own mode-2 grants at the receiver through the
  symmetric per-link chains). The handshake is a synchronous "genie" by
  default; `pc5RrcMode = "overTheAir"` runs it as real
  `SlLinkEstablishRequest`/`Response` messages over a reserved TM SL-SRB
  (DRB 63), with the triggering packet held until the link is up and the
  establishment latency observable.

- **PSFCH HARQ feedback.** Pool-configured PSFCH slots
  (`psfchPeriod` ∈ {1,2,4}) carry per-TB ACK/NACK: unicast always,
  groupcast per SLRB — option 1 (distance-gated NACK-only, `mcr`) or
  option 2 (per-member ACK/NACK on member-derived resource indices).
  The TX HARQ entity parks TBs awaiting feedback, retransmits on NACK
  (up to `harqMaxRtx`), and resolves missing feedback (DTX, e.g. from
  half-duplex loss of the PSFCH slot) per `psfchDtxPolicy`. Feedback
  matches blind retransmission delivery at ~33% fewer transmissions in
  the knee benchmark. PSFCH transmissions occupy abstract per-slot
  resource indices; co-resource feedbacks interfere (documented
  simplification — no PRB mapping or code multiplexing).

- **SL QoS ("SL-SDAP") and LCP.** Per-destination `SlSdapEntity` modules
  map PC5 QoS flows to SLRBs (PFI from a `QfiReq` tag, DSCP fallback, or
  default; entries in `slrbConfig`/`unicastSlrbDefaults` carry
  `pfi`/`pqi`/`isDefault`). The sidelink MAC fills one TB per TX occasion
  across the destination's backlogged SLRBs in PQI-priority order
  (TS 23.287 priority subset + `pqiPriorityOverrides`).

- **CBR-based congestion control.** A `cbrConfig` level table (TS 38.214
  §8.1.6-style) caps MCS, subchannels and TX power per measured-CBR
  range and enforces a channel-occupancy-ratio limit by skipping TX
  occasions.

- **Examples.** `basic/`: unicast UM/AM pairs, PSFCH-at-the-knee, a
  two-flow QoS priority demo, and the over-the-air handshake;
  `highway/`: a 5-vehicle platoon with groupcast options 1 and 2, and a
  congested variant demonstrating CBR-driven adaptation (CBR 0.60→0.52,
  short-range PRR 0.97→0.99 vs an uncontrolled run).

#### Scope and limitations (after SL-2)

- Mode 1 (gNB-scheduled sidelink resources) is not yet supported; only
  Mode 2 (UE-autonomous), `random`, and `static` allocation are available.
- Uu/PC5 path selection is a static rule ("PC5 whenever the peer is
  SL-capable"); a policy hook is planned for SL-3.
- Resources are single-slot; there is no multi-slot/multi-resource grant
  chaining, re-evaluation/pre-emption, or Rel-17 inter-UE coordination.
- Unicast links have no RLF/keepalive and no PC5-S security; the SL-SRB
  is a single pre-provisioned TM bearer (SRB0-3 collapsed).
- PSFCH BLER curves are not SL-specific (the coarse MCS→CQI map feeds
  the Uu curves); option-1 NACKs interfere instead of superposing.
- Synchronization is ideal; there is no sidelink DRX and no in-band
  emission model.
